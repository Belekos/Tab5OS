"""Verify a completed Linux Tab5OS firmware build against its source manifest."""
import argparse, csv, datetime, difflib, hashlib, importlib.util, json, re, struct, subprocess, sys
from pathlib import Path
sys.dont_write_bytecode=True
from compile_linux import manifest

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--environment',type=Path,required=True)
    args=ap.parse_args();env=args.environment.resolve();qa=env/'qa';build=env/'build'
    result=json.loads((qa/'build-result.json').read_text());root=Path(result['sketch']);name=root.name+'.ino'
    if result.get('exitCode')!=0 or result.get('compilerExitCode',result.get('exitCode'))!=0:raise RuntimeError('Complete compiler must succeed before verification')
    initial=json.loads((qa/'source-manifest-start.json').read_text())['files'];final=manifest(root)
    if initial!=final or not result.get('sourcesUnchanged'):raise RuntimeError('Source manifest changed')
    (qa/'source-manifest-final.json').write_text(json.dumps({'root':str(root),'files':final},indent=2)+'\n')
    copies=[];mismatches=[]
    for item in final:
        source=root/item['path'];copy=build/'sketch'/item['path']
        if copy.is_file():
            original=source.read_bytes().removeprefix(b'\xef\xbb\xbf');copied=copy.read_bytes().removeprefix(b'\xef\xbb\xbf')
            if copied.startswith(b'#line 1 '):copied=copied.split(b'\n',1)[1]
            if original!=copied:mismatches.append(item['path'])
            copies.append(item['path'])
    if mismatches:raise RuntimeError('Arduino source copy mismatch: '+repr(mismatches))
    original_sketch=(root/(root.name+'.ino')).read_bytes().removeprefix(b'\xef\xbb\xbf').replace(b'\r\n',b'\n')
    merged_file=build/'sketch'/(name+'.cpp.merged')
    merged_sketch=re.sub(rb'^#line [^\n]*\n',b'',merged_file.read_bytes().removeprefix(b'\xef\xbb\xbf').replace(b'\r\n',b'\n'),flags=re.M)
    if merged_sketch not in [original_sketch,original_sketch+b'\n']:raise RuntimeError('Merged sketch differs from original')
    generated_file=build/'sketch'/(name+'.cpp')
    generated_sketch=re.sub(rb'^#line [^\n]*\n',b'',generated_file.read_bytes().removeprefix(b'\xef\xbb\xbf').replace(b'\r\n',b'\n'),flags=re.M)
    generated_exact=generated_sketch in [original_sketch,original_sketch+b'\n']
    source_lines=original_sketch.splitlines();generated_lines=generated_sketch.splitlines()
    generated_changes=[(tag,a,b,c,d) for tag,a,b,c,d in difflib.SequenceMatcher(None,source_lines,generated_lines,autojunk=False).get_opcodes() if tag!='equal']
    generated_preserved=all(tag=='insert' and all(not line.strip() or line.rstrip().endswith(b';') for line in generated_lines[c:d]) for tag,a,b,c,d in generated_changes)
    if not generated_preserved:raise RuntimeError('Generated sketch changes source beyond inserted Arduino prototypes')
    prototype_count=sum(sum(bool(line.strip()) for line in generated_lines[c:d]) for tag,a,b,c,d in generated_changes)

    elf=build/(name+'.elf');binary=build/(name+'.bin');elfdata=elf.read_bytes()
    if elfdata[:6]!=b'\x7fELF\x01\x01':raise RuntimeError('Expected ELF32 little endian')
    if struct.unpack_from('<H',elfdata,18)[0]!=243:raise RuntimeError('Expected RISC-V machine')
    flags=struct.unpack_from('<I',elfdata,36)[0]
    if flags&6!=2:raise RuntimeError('Expected RISC-V single-float ABI')
    partitions=[]
    for row in csv.reader((build/'partitions.csv').read_text().splitlines()):
        if len(row)>=5 and not row[0].lstrip().startswith('#'):
            partitions.append({'name':row[0].strip(),'type':row[1].strip(),'subtype':row[2].strip(),'offset':int(row[3].strip(),0),'bytes':int(row[4].strip(),0)})
    expected_partitions=[
        {'name':'nvs','type':'data','subtype':'nvs','offset':0x9000,'bytes':0x5000},
        {'name':'otadata','type':'data','subtype':'ota','offset':0xe000,'bytes':0x2000},
        {'name':'app0','type':'app','subtype':'ota_0','offset':0x10000,'bytes':0x640000},
        {'name':'app1','type':'app','subtype':'ota_1','offset':0x650000,'bytes':0x640000},
        {'name':'spiffs','type':'data','subtype':'spiffs','offset':0xc90000,'bytes':0x360000},
        {'name':'coredump','type':'data','subtype':'coredump','offset':0xff0000,'bytes':0x10000}]
    if partitions!=expected_partitions:raise RuntimeError('Partition table differs from the preserved layout')
    app_sizes=[p['bytes'] for p in partitions if p['type']=='app']
    if not app_sizes or binary.stat().st_size>min(app_sizes):raise RuntimeError('Image exceeds OTA partition')
    esptool=env/'data/packages/m5stack/tools/esptool_py/5.3.0/esptool'
    proc=subprocess.run([str(esptool),'--chip','esp32p4','image-info',str(binary)],capture_output=True,text=True,check=True)
    info=proc.stdout+proc.stderr;(qa/'image-info.log').write_text(info)
    if 'ESP32-P4' not in info or not re.search(r'Checksum:[^\n]*\(valid\)',info) or not re.search(r'Validation hash:[^\n]*\(valid\)',info) or '(invalid)' in info or sha(elf) not in info.lower():raise RuntimeError('ESP image validation or embedded ELF hash failed')
    version_match=re.search(r'APP_VERSION\[\]\s*=\s*"([^"]+)"',(root/(root.name+'.ino')).read_text())
    if not version_match:raise RuntimeError('APP_VERSION not found')
    version=version_match[1]
    if (version+'\0').encode() not in binary.read_bytes():raise RuntimeError('APP_VERSION not present in firmware')
    nm=env/'data/packages/m5stack/tools/esp-rv32/2601/bin/riscv32-esp-elf-nm'
    symbols=subprocess.run([str(nm),'--defined-only',str(elf)],capture_output=True,text=True,check=True).stdout
    (qa/'firmware-nm.log').write_text(symbols)
    obj=build/'sketch/Tab5CryptoLockCompat.c.o'
    obj_symbols=subprocess.run([str(nm),'--defined-only',str(obj)],capture_output=True,text=True,check=True).stdout
    required=['esp_crypto_ecdsa_lock_acquire','esp_crypto_ecdsa_lock_release','esp_crypto_sha_aes_lock_acquire','esp_crypto_sha_aes_lock_release']
    linked_map=(build/(name+'.map')).read_text()
    crypto_checks=[]
    for symbol in required:
        count=len(re.findall(r'^\S+\s+[Tt]\s+'+symbol+r'$',symbols,re.M))
        defined_in_object=bool(re.search(r'^\S+\s+[Tt]\s+'+symbol+r'$',obj_symbols,re.M))
        origins=re.findall(r'^\s*\.text\.'+symbol+r'\s*\n([^\n]+)',linked_map,re.M)
        linked_from_object=any('Tab5CryptoLockCompat.c.o' in line for line in origins)
        crypto_checks.append({'symbol':symbol,'definitionCount':count,'definedInCompatibilityObject':defined_in_object,'mapOriginMatches':linked_from_object})
    if not all(c['definitionCount']==1 and c['definedInCompatibilityObject'] and c['mapOriginMatches'] for c in crypto_checks):raise RuntimeError('Crypto-lock provenance check failed: '+json.dumps(crypto_checks))
    if re.search(r'libesp_security\.a\(esp_crypto_lock\.c\.obj\)',linked_map):raise RuntimeError('Unexpected SDK crypto-lock archive object in map')
    # The exact reviewed ESP-IDF 5.5.4 host allocator must replace the SDK member.
    mesh_object=build/'sketch/Tab5MeshAllocator.c.o'
    mesh_symbols=subprocess.run([str(nm),'--defined-only',str(mesh_object)],capture_output=True,text=True,check=True).stdout
    mesh_required=['bt_osi_mem_malloc','bt_osi_mem_calloc','bt_osi_mem_free','bt_osi_mem_malloc_internal','bt_osi_mem_calloc_internal','bt_osi_mem_free_internal']
    mesh_ranges=[(int(m[1],16),int(m[1],16)+int(m[2],16)) for m in re.finditer(r'^\s+\.iram1\.\S+\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+[^\n]*Tab5MeshAllocator\.c\.o\)?\s*$',linked_map,re.M) if int(m[1],16)>0]
    mesh_checks=[]
    for symbol in mesh_required:
        object_count=len(re.findall(r'^\S+\s+[Tt]\s+'+symbol+r'$',mesh_symbols,re.M))
        definitions=re.findall(r'^(\S+)\s+[Tt]\s+'+symbol+r'$',symbols,re.M)
        count=len(definitions)
        matches=count==1 and any(start<=int(definitions[0],16)<end for start,end in mesh_ranges)
        mesh_checks.append({'symbol':symbol,'objectDefinitionCount':object_count,'linkedDefinitionCount':count,'linkedFromReviewedObject':matches if count else None,'eliminatedByLinker':count==0})
    if not all(c['objectDefinitionCount']==1 and c['linkedDefinitionCount']<=1 and (not c['linkedDefinitionCount'] or c['linkedFromReviewedObject']) for c in mesh_checks):raise RuntimeError('Bluetooth allocator provenance failed: '+json.dumps(mesh_checks))
    if not all(c['linkedDefinitionCount']==1 for c in mesh_checks[1:3]):raise RuntimeError('Expected Bluetooth generic calloc/free in final firmware')
    if re.search(r'libbt\.a\(bt_osi_mem\.c\.obj\)',linked_map):raise RuntimeError('Unexpected SDK Bluetooth allocator member in final link')
    (qa/'mesh-allocator-object-nm.log').write_text(mesh_symbols)
    objdump=nm.with_name('riscv32-esp-elf-objdump')
    object_table=subprocess.run([str(objdump),'-t',str(elf)],capture_output=True,text=True,check=True).stdout
    from verify_native_admission import verify as verify_native_admission
    heap_probe_check=verify_native_admission(env,elf,objdump)
    (qa/'heap-probe-linked.json').write_text(json.dumps(heap_probe_check,indent=2)+'\n')
    from verify_lvgl_memory import verify as verify_lvgl_memory
    lvgl_memory_check=verify_lvgl_memory(env,root,elf,objdump)
    (qa/'lvgl-memory-linked.json').write_text(json.dumps(lvgl_memory_check,indent=2)+'\n')
    from verify_cache_sync import verify as verify_cache_sync
    cache_sync_check=verify_cache_sync(elf,objdump,qa/'cache-sync-linked-disassembly.log')
    (qa/'cache-sync-linked.json').write_text(json.dumps(cache_sync_check,indent=2)+'\n')
    if not cache_sync_check['passed']:raise RuntimeError('Cache-sync adapter linkage failed: '+repr(cache_sync_check['failures']))
    from verify_dsi_cache_safe import verify as verify_native_dsi
    dsi_check=verify_native_dsi(elf,objdump)
    (qa/'dsi-cache-safe-linked.json').write_text(json.dumps(dsi_check,indent=2)+'\n')
    if not dsi_check.get('passed'):raise RuntimeError('Native DSI/DW cache-safety check failed: '+repr(dsi_check.get('failures')))
    dsi_work=qa/'firmware/dsi-cache-safe'
    dsi_meta=json.loads((dsi_work/'report.json').read_text())
    if not dsi_meta.get('passed') or sha(root.parent/'tools/build_linux/prepare_dsi_cache_safe.py')!=dsi_meta['patchScriptSha256']:
        raise RuntimeError('Native DSI preparation metadata changed')
    for source_name,digest in dsi_meta['generatedSha256'].items():
        if sha(dsi_work/'src'/source_name)!=digest:raise RuntimeError('Generated native DSI source changed: '+source_name)
    for archive_name,digest in dsi_meta['sdkArchiveSha256'].items():
        if sha(Path(dsi_meta['sdk'])/'lib'/archive_name)!=digest:raise RuntimeError('Installed native SDK archive changed: '+archive_name)
    for archive_name,row in dsi_meta['archives'].items():
        archive=Path(row['path'])
        if sha(archive)!=row['sha256']:raise RuntimeError('Isolated native archive changed: '+archive_name)
        member='esp_lcd_panel_dpi.o' if archive_name=='libesp_lcd.a' else 'dw_gdma.o'
        if str(archive)+'('+member+')' not in linked_map:raise RuntimeError('Isolated native archive not linked: '+archive_name)
    for native_member in ('esp_lcd_panel_dpi.c.obj','dw_gdma.c.obj'):
        if '('+native_member+')' in linked_map:raise RuntimeError('Unpatched native SDK member still linked: '+native_member)
    # Inspect the actual linked call, not only the compile flags or an object
    # relocation: a missing --wrap silently leaves the observer inactive.
    display_names=['__wrap_esp_intr_alloc','esp_intr_alloc','_ZN12_GLOBAL__N_113observeBridgeEPv','mipi_dsi_bridge_isr_handler','esp_timer_get_time','tab5_display_underrun_isr']
    display_rows={symbol:[line for line in object_table.splitlines() if line.split() and line.split()[-1]==symbol] for symbol in display_names}
    if not all(len(rows)==1 for rows in display_rows.values()):raise RuntimeError('Missing or duplicate linked display symbol: '+json.dumps(display_rows))
    for symbol in display_names[2:]:
        if '.iram0.text' not in display_rows[symbol][0].split():raise RuntimeError('Display interrupt path outside IRAM: '+symbol)
    fields=['originalHandler','originalArgument','active','installing','sequence','allocatedFlags','interrupts','underruns','lastStatus','lastUsLow','lastUsHigh']
    # Anonymous-namespace symbols can share their mangled name with another
    # translation unit. Select only this object's linker-map contributions.
    observer_ranges=[(int(m[1],16),int(m[1],16)+int(m[2],16)) for m in re.finditer(r'^\s+\.dram1\.\S+\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+[^\n]*Tab5DisplaySignals\.cpp\.o\)?\s*$',linked_map,re.M) if int(m[1],16)>0]
    if sum(end-start for start,end in observer_ranges)!=44:raise RuntimeError('Unexpected internal display-state allocation: '+repr(observer_ranges))
    display_data=[]
    for field in fields:
        symbol='_ZN12_GLOBAL__N_1'+str(len(field))+field+'E'
        rows=[line for line in object_table.splitlines() if line.split() and line.split()[-1]==symbol and any(start<=int(line.split()[0],16)<end for start,end in observer_ranges)]
        if len(rows)!=1 or not {'.dram0.data','.dram1.data','.dram0.bss','.dram1.bss'}.intersection(rows[0].split()):raise RuntimeError('Display observer state outside internal DRAM: '+field)
        display_data.append({'field':field,'row':rows[0]})
    display_disassembly={}
    for symbol in ('esp_lcd_new_panel_dpi','__wrap_esp_intr_alloc'):
        display_disassembly[symbol]=subprocess.run([str(objdump),'-d','--disassemble='+symbol,str(elf)],capture_output=True,text=True,check=True).stdout
    if not re.search(r'\b(?:jal|jalr|j)\b[^\n]*<__wrap_esp_intr_alloc>',display_disassembly['esp_lcd_new_panel_dpi']):raise RuntimeError('SDK DPI driver does not call the linked interrupt wrapper')
    if re.search(r'\b(?:jal|jalr|j)\b[^\n]*<esp_intr_alloc>',display_disassembly['esp_lcd_new_panel_dpi']):raise RuntimeError('SDK DPI driver bypasses the interrupt wrapper')
    if not re.search(r'\b(?:jal|jalr|j)\b[^\n]*<esp_intr_alloc>',display_disassembly['__wrap_esp_intr_alloc']):raise RuntimeError('Interrupt wrapper does not call SDK interrupt allocator')
    (qa/'display-linked-disassembly.log').write_text('\n'.join(display_disassembly.values()))
    display_link={'driverCallsWrapper':True,'wrapperCallsSdkAllocator':True,'interruptPathInIram':True,'sharedStateInternalDram':True,'symbols':display_rows,'fields':display_data,'physicalDeviceTested':False}
    refresh_matches=re.findall(r'^\S+\s+[Tt]\s+(\S*onRefresh\S*)$',symbols,re.M)
    if len(refresh_matches)!=1:raise RuntimeError('Expected one native display refresh callback')
    refresh_symbol=refresh_matches[0]
    refresh_rows=[line for line in object_table.splitlines() if line.split() and line.split()[-1]==refresh_symbol]
    if len(refresh_rows)!=1 or '.iram0.text' not in refresh_rows[0].split():raise RuntimeError('Native refresh callback outside IRAM')
    refresh_data=[]
    for field in ('refreshes','lastRefreshUs','maxRefreshGapUs'):
        symbol='_ZN12_GLOBAL__N_1'+str(len(field))+field+'E'
        rows=[line for line in object_table.splitlines() if line.split() and line.split()[-1]==symbol]
        if len(rows)!=1 or not {'.dram0.data','.dram1.data','.dram0.bss','.dram1.bss'}.intersection(rows[0].split()):raise RuntimeError('Native refresh data outside internal DRAM: '+field)
        refresh_data.append({'field':field,'row':rows[0]})
    refresh_disassembly=subprocess.check_output([str(objdump),'-d','--disassemble='+refresh_symbol,str(elf)],text=True)
    refresh_targets=re.findall(r'\b(?:jal|jalr|j)\b[^\n]*<([^>]+)>',refresh_disassembly)
    if any(not (target=='esp_timer_get_time' or target.startswith(refresh_symbol)) for target in refresh_targets):raise RuntimeError('Unexpected native refresh callback call: '+repr(refresh_targets))
    if 'esp_timer_get_time' not in refresh_targets:raise RuntimeError('Native refresh timing call not found')
    (qa/'display-refresh-disassembly.log').write_text(refresh_disassembly)
    display_link['nativeRefreshCallback']={'symbol':refresh_symbol,'inIram':True,'sharedStateInternalDram':True,'fields':refresh_data,'callTargets':refresh_targets,'timerInIram':True}
    constant_checks=[]
    for symbol,expected in [('sha1_dvs',11352),('xz_crc32_table',1024),('xz_crc64_table',2048)]:
        rows=[line for line in object_table.splitlines() if line.split() and line.split()[-1]==symbol]
        ro_nm=bool(re.search(r'^\S+\s+[Rr]\s+'+symbol+r'$',symbols,re.M))
        good=len(rows)==1 and '.flash.rodata' in rows[0].split() and int(rows[0].split()[-2],16)==expected and ro_nm
        constant_checks.append({'symbol':symbol,'expectedBytes':expected,'readOnlyFlash':good,'objdumpRows':rows})
    if not all(c['readOnlyFlash'] for c in constant_checks):raise RuntimeError('Read-only memory-table placement failed: '+json.dumps(constant_checks))
    size_tool=nm.with_name('riscv32-esp-elf-size')
    size_output=subprocess.run([str(size_tool),'-A',str(elf)],capture_output=True,text=True,check=True).stdout
    (qa/'firmware-sections.log').write_text(size_output)
    sections={m[1]:int(m[2]) for m in re.finditer(r'^(\.[^\s]+)\s+(\d+)\s+\d+\s*$',size_output,re.M)}
    # Use the preserved release baseline, not constants copied across versions.
    baseline=json.loads((root.parent/('tools/audit-'+version.replace('.',''))/'build-baseline.json').read_text())
    previous_sections={name:baseline['sections'][name] for name in ('.dram0.data','.dram1.data','.dram0.bss','.dram1.bss','.iram0.text','.tcm.text','.tcm.data','.rtc_noinit')}
    comparisons={name:{'baseline':old,'current':sections.get(name,0),'savedBytes':old-sections.get(name,0)} for name,old in previous_sections.items()}
    memory_link={'constantTables':constant_checks,'constantTableBytesInFlash':14424,'constantTableBytesNewlyMovedToFlash':0,'allSections':sections,'sectionComparisons':comparisons,'dataBssSavedBytes':sum(comparisons[name]['savedBytes'] for name in ('.dram0.data','.dram1.data','.dram0.bss','.dram1.bss')),'baselineVersion':baseline['baselineVersion'],'baselineArchiveSha256':baseline['archiveSha256'],'scope':'Static linked sections only; runtime free heap is measured on the device, not inferred here.'}
    # Bind the decoder actually compiled, not merely an installed library copy.
    jpeg_patch=json.loads((qa/'firmware/m5gfx-jpeg-patch.json').read_text())
    decoder=Path(jpeg_patch['isolatedCopy'])/jpeg_patch['file']
    if sha(decoder)!=jpeg_patch['patchedSha256']:raise RuntimeError('Patched JPEG decoder changed')
    decoder_dependencies=list(build.rglob('lgfx_tjpgd.c.d'))
    if len(decoder_dependencies)!=1:raise RuntimeError('Expected one compiled JPEG decoder dependency file')
    dependency_text=decoder_dependencies[0].read_text().replace('\\\n',' ')
    if str(decoder) not in dependency_text:raise RuntimeError('Compiled JPEG decoder is not the checked isolated copy')
    (qa/'jpeg-decoder-dependency.d').write_bytes(decoder_dependencies[0].read_bytes())
    # Bind the errno-only FS backport to the actual compiled and linked SDK
    # library. A patched file on disk is not proof the resolver selected it.
    fs_meta_file=qa/'firmware/fs-errno-patch.json'
    fs_meta=json.loads(fs_meta_file.read_text())
    fs_source=Path(fs_meta['source']).resolve();fs_isolated=Path(fs_meta['isolated']).resolve()
    fs_patch_script=root.parent/'tools/build_linux/prepare_fs_errno.py'
    if not fs_meta.get('passed') or not fs_meta.get('cachedSdkUnmodified') or sha(fs_patch_script)!=fs_meta['patchScriptSha256']:
        raise RuntimeError('FS backport provenance check failed')
    original_vfs=fs_source/'src/vfs_api.cpp';patched_vfs=fs_isolated/'src/vfs_api.cpp'
    if sha(original_vfs)!=fs_meta['upstreamSha256'] or sha(patched_vfs)!=fs_meta['patchedSha256']:
        raise RuntimeError('Original or patched FS source changed')
    original_fs={p.relative_to(fs_source).as_posix():sha(p) for p in fs_source.rglob('*') if p.is_file()}
    isolated_fs={p.relative_to(fs_isolated).as_posix():sha(p) for p in fs_isolated.rglob('*') if p.is_file()}
    expected_fs=dict(original_fs);expected_fs['src/vfs_api.cpp']=fs_meta['patchedSha256']
    if isolated_fs!=expected_fs:raise RuntimeError('Isolated FS contains unexpected changes')
    fs_bindings=[];fs_evidence={}
    for filename in ('FS.cpp','vfs_api.cpp'):
        dependencies=list(build.rglob(filename+'.d'))
        if len(dependencies)!=1:raise RuntimeError('Expected one compiled FS dependency file: '+filename)
        dep=dependencies[0];text=dep.read_text().replace('\\\n',' ')
        if str(fs_isolated/'src'/filename) not in text:raise RuntimeError('Compiler did not select isolated FS source: '+filename)
        objfile=dep.with_suffix('.o')
        if not objfile.is_file():raise RuntimeError('Compiled FS object missing: '+filename)
        evidence_name='fs-'+filename+'.d';(qa/evidence_name).write_bytes(dep.read_bytes());fs_evidence[evidence_name]=sha(qa/evidence_name)
        fs_bindings.append({'source':filename,'isolatedSourceSha256':sha(fs_isolated/'src'/filename),'objectSha256':sha(objfile),'dependencySha256':sha(dep)})
    stale_fs_dependencies=[]
    fs_dependency_users=0
    for dep in build.rglob('*.d'):
        if dep.name.endswith('.libsdetect.d'):continue
        text=dep.read_text()
        if str(fs_source/'src')+'/' in text or '/libraries/FS/src/' in text:stale_fs_dependencies.append(str(dep.relative_to(build)))
        if str(fs_isolated/'src')+'/' in text:fs_dependency_users+=1
    if stale_fs_dependencies:raise RuntimeError('Unpatched SDK FS headers/source still compiled: '+repr(stale_fs_dependencies))
    fs_symbols=('_ZN7VFSImpl4openEPKcS1_b','_ZN11VFSFileImpl5flushEv')
    for symbol in fs_symbols:
        if len(re.findall(r'^\S+\s+[Tt]\s+'+symbol+r'$',symbols,re.M))!=1:raise RuntimeError('Missing or duplicate linked FS method: '+symbol)
        origins=re.findall(r'^\s*\.text\.'+symbol+r'\s*\n([^\n]+)',linked_map,re.M)
        if len(origins)!=1 or 'libraries/FS/objs.a(vfs_api.cpp.o)' not in origins[0]:raise RuntimeError('Linked FS method has unexpected origin: '+symbol)
    fs_check={'passed':True,'patch':fs_meta,'filesChecked':len(isolated_fs),'compiledBindings':fs_bindings,'dependencyUsers':fs_dependency_users,'unpatchedDependencyUsers':stale_fs_dependencies,'linkedMethods':list(fs_symbols),'scope':'Exact pinned source metadata/backport and compiler/linker selection; no physical SD behavior asserted.'}
    from verify_m5unified_audio import check as verify_speaker
    speaker_check,speaker_evidence=verify_speaker(env,linked_map,elf)
    artifacts=[]
    for suffix in ['.bin','.elf','.map','.merged.bin','.bootloader.bin','.partitions.bin']:
        f=build/(name+suffix)
        if f.is_file():artifacts.append({'suffix':suffix,'file':f.name,'bytes':f.stat().st_size,'sha256':sha(f)})
    if not {'.bin','.elf','.map'} <= {item['suffix'] for item in artifacts}:raise RuntimeError('Required final firmware artifacts missing')
    report={'passed':True,'version':version,'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'buildExitCode':result['exitCode'],'sourcesUnchanged':True,'build':result,'sourceFilesChecked':len(final),'arduinoCopiesChecked':len(copies),'sourceCopyMismatches':mismatches,'mergedSketchMatches':True,'generatedSketchMatchesExactly':generated_exact,'generatedSketchSourcePreserved':generated_preserved,'arduinoAddedPrototypeLines':prototype_count,'generatedSketchSha256':sha(generated_file),'toolchain':'Arduino M5Stack 3.3.9, GCC RISC-V 14.2.0 (esp-rv32 2601), Linux','toolVersions':json.loads((qa/'toolchain-versions.json').read_text()),'elf32LittleEndian':True,'elfMachine':'RISC-V','elfFlags':hex(flags),'singleFloatAbi':True,'imageChip':'ESP32-P4','imageChecksumValid':True,'imageValidationHashValid':True,'elfEmbeddedShaMatches':True,'appVersionPresent':True,'firmwareBytes':binary.stat().st_size,'otaPartitionBytes':min(app_sizes),'otaFreeBytes':min(app_sizes)-binary.stat().st_size,'partitions':partitions,'partitionsUnchangedFromBaseline':partitions==baseline['partitions'],'artifacts':artifacts,'cryptoLockChecks':crypto_checks,'cryptoLockSourceSha256':sha(root/'Tab5CryptoLockCompat.c'),'cryptoLockObjectSha256':sha(obj),'dependencyPatch':json.loads((qa/'lvgl-arabic-patch.json').read_text()),'objectFiles':len(list(build.rglob('*.o'))),'physicalDeviceTested':False,'evidenceSha256':{f:sha(qa/f) for f in ['build-result.json','build.log','compile-args.json','source-manifest-start.json','source-manifest-end.json','source-manifest-final.json','image-info.log','firmware-nm.log']+[x for x in ['dependency-downloads.json','gcc-source-fixes.json','gcc-integer-types.json','vendor-syntax-gcc.json'] if (qa/x).is_file()]}}
    report['sourceHashes']={root.name+'/'+row['path']:row['sha256'] for row in final}
    report['testSourceHashes']={str(path.relative_to(root.parent)):sha(path) for path in [Path(__file__),root.parent/'tools/build_linux/compile_linux.py',root.parent/'tools/build_linux/verify_cache_sync.py',root.parent/'tools/build_linux/verify_m5unified_audio.py',root.parent/'tools/build_linux/verify_heap_probe.py',root.parent/'tools/build_linux/verify_native_admission.py',root.parent/'tools/build_linux/verify_lvgl_memory.py']}
    report['nativeAdmissionLinkChecks']=heap_probe_check
    report['evidenceSha256']['heap-probe-linked-disassembly.log']=sha(qa/'heap-probe-linked-disassembly.log')
    report['evidenceSha256']['heap-probe-linked.json']=sha(qa/'heap-probe-linked.json')
    report['evidenceSha256']['heap-probe-sdk.json']=sha(qa/'heap-probe-sdk.json')
    report['lvglMemoryLinkChecks']=lvgl_memory_check
    report['evidenceSha256']['lvgl-memory-linked-disassembly.log']=sha(qa/'lvgl-memory-linked-disassembly.log')
    report['evidenceSha256']['lvgl-memory-linked.json']=sha(qa/'lvgl-memory-linked.json')
    report['evidenceSha256']['lvgl-tlsf-dependency.d']=sha(qa/'lvgl-tlsf-dependency.d')
    report['cacheSyncLinkChecks']=cache_sync_check
    report['evidenceSha256']['cache-sync-linked-disassembly.log']=sha(qa/'cache-sync-linked-disassembly.log')
    report['evidenceSha256']['cache-sync-linked.json']=sha(qa/'cache-sync-linked.json')
    report['nativeDsiCacheSafety']=dsi_check
    report['nativeDsiPreparation']=dsi_meta
    report['evidenceSha256']['dsi-cache-safe-linked.json']=sha(qa/'dsi-cache-safe-linked.json')
    report['evidenceSha256']['firmware/dsi-cache-safe/report.json']=sha(dsi_work/'report.json')
    report['m5UnifiedAudioBackport']=speaker_check
    report['evidenceSha256'].update(speaker_evidence)
    report['fsErrnoBackport']=fs_check
    report['evidenceSha256']['firmware/fs-errno-patch.json']=sha(fs_meta_file)
    report['evidenceSha256'].update(fs_evidence)
    report['meshAllocatorChecks']=mesh_checks
    report['meshAllocatorObjectSha256']=sha(mesh_object)
    report['meshAllocatorSourceHashes']={name:sha(root/name) for name in ['Tab5MeshAllocator.c','Tab5MeshAllocator.h','Tab5MeshAllocatorEspIdf554.inc']}
    report['meshSdkAllocatorArchiveMemberAbsent']=True
    report['displaySignalLinkChecks']=display_link
    report['displaySignalSourceHashes']={name:sha(root/name) for name in ('Tab5DisplaySignals.cpp','Tab5DisplaySignals.h')}
    report['evidenceSha256']['display-linked-disassembly.log']=sha(qa/'display-linked-disassembly.log')
    report['evidenceSha256']['display-refresh-disassembly.log']=sha(qa/'display-refresh-disassembly.log')
    report['memoryLinkChecks']=memory_link
    report['evidenceSha256']['mesh-allocator-object-nm.log']=sha(qa/'mesh-allocator-object-nm.log')
    report['evidenceSha256']['firmware-sections.log']=sha(qa/'firmware-sections.log')
    report['decoderSourceSha256']=sha(decoder)
    report['jpegDecoderPatch']=jpeg_patch
    report['decoderDependencySha256']=sha(decoder_dependencies[0])
    report['evidenceSha256']['firmware/m5gfx-jpeg-patch.json']=sha(qa/'firmware/m5gfx-jpeg-patch.json')
    report['evidenceSha256']['jpeg-decoder-dependency.d']=sha(qa/'jpeg-decoder-dependency.d')
    warning_file=qa/'compiler-warning-pass.json'
    if warning_file.is_file():
        warning=json.loads(warning_file.read_text())
        if not warning.get('passed') or warning.get('version')!=version or not warning.get('units'):
            raise RuntimeError('Current compiler warning pass failed')
        warning_script=root.parent/'tools/build_linux/warnings_current.py'
        if warning.get('scriptSha256')!=sha(warning_script):raise RuntimeError('Warning-pass script changed after execution')
        for relative,h in warning['sourceHashes'].items():
            source=(root.parent/relative).resolve()
            if not source.is_relative_to(root.parent.resolve()) or sha(source)!=h:raise RuntimeError('Warning-pass source changed: '+relative)
        report['evidenceSha256']['compiler-warning-pass.json']=sha(warning_file)
        for unit,row in warning['units'].items():
            relative='warning-pass/'+unit+'.log'
            if sha(qa/relative)!=row['logSha256']:raise RuntimeError('Warning log changed: '+unit)
            report['evidenceSha256'][relative]=sha(qa/relative)
        report['compilerWarningPass']={'passed':True,'unitCount':len(warning['units']),'warningCount':sum(v['warningCount'] for v in warning['units'].values()),'suppressionRemoved':warning['suppressionRemoved'],'flags':warning['addedFlags'],'limitations':warning['limitations']}
    warning_comparison_file=qa/'compiler-warning-comparison.json'
    if warning_comparison_file.is_file():
        comparison=json.loads(warning_comparison_file.read_text())
        comparison_script=root.parent/'tools/build_linux/compare_warnings.py'
        if not comparison.get('passed') or comparison.get('version')!=version or comparison.get('baselineArchiveSha256')!=baseline['archiveSha256']:
            raise RuntimeError('Current/baseline warning comparison failed')
        if comparison.get('scriptSha256')!=sha(comparison_script):raise RuntimeError('Warning comparison script changed after execution')
        if comparison['currentWarningCount']!=report.get('compilerWarningPass',{}).get('warningCount'):
            raise RuntimeError('Warning comparison no longer matches current warning pass')
        report['evidenceSha256']['compiler-warning-comparison.json']=sha(warning_comparison_file)
        for unit,row in comparison['units'].items():
            relative='warning-baseline/'+unit+'.log'
            if sha(qa/relative)!=row['baselineLogSha256']:raise RuntimeError('Baseline warning log changed: '+unit)
            report['evidenceSha256'][relative]=sha(qa/relative)
        report['compilerWarningComparison']={'passed':True,'baselineVersion':comparison['baselineVersion'],'baselineWarningCount':comparison['baselineWarningCount'],'currentWarningCount':comparison['currentWarningCount'],'allCategoryCountsUnchanged':all(not delta for row in comparison['units'].values() for delta in row['categoryDeltas'].values()),'baselineSourceFilesAuthenticated':comparison['baselineSourceFilesAuthenticated'],'limitations':comparison['limitations']}
    warning_review=qa/'compiler-warning-review.json'
    if warning_review.is_file():
        review=json.loads(warning_review.read_text())
        if review.get('version')!=version or not all(row.get('identicalToBaseline') for row in review.get('reviewedDeclarations',[])):
            raise RuntimeError('Warning source review is not current')
        report['compilerWarningSourceReview']=review
        report['evidenceSha256']['compiler-warning-review.json']=sha(warning_review)
    target=qa/('firmware-verification-'+version.replace('.', '')+'.json');target.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report[k] for k in ['passed','version','firmwareBytes','otaFreeBytes','objectFiles','sourcesUnchanged','physicalDeviceTested']},indent=2))
if __name__=='__main__':main()
