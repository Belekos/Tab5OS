"""Current .28 executions; prior assertions remain enforced with explicit paths."""
from feature_contracts_0827 import *
import feature_contracts_0827 as previous
import feature_contracts_0826 as older
import inspect

NEW_REPORTS = ('battery-protection-0828.json', 'battery-ui-0828.json', 'battery-integration-0828.json', 'vault-buffers-0828.json',
               'crypto-lock-0828.json', 'crypto-lock-target-0828.json',
               'crypto-lock-link-0828.json', 'writing-recovery-0828.json', 'git-target-0828.json',
               'compatibility-0828.json', 'compatibility-target-0828.json')
HARNESS_PATHS = {
    name: {before: after.replace('0827','0828') for before, after in mapping.items()}
    for name, mapping in previous.HARNESS_PATHS.items()
}
HARNESS_PATHS.update({
    'git-0822.json': {'run.py':'run_0828.py','git_cli.cpp':'git_cli_0828.cpp'},
    'git-journal-0822.json': {'run.py':'run_0828.py'},
    'git-https-0822.json': {'https_remote.py':'https_remote_0828.py','run.py':'run_0828.py',
                          'libgit2-preflight-0822':'libgit2-preflight-0826','git_cli.cpp':'git_cli_0828.cpp'},
    'git-view-0822.json': {'git-view.cpp':'git-view-0828.cpp','run_git_view.py':'run_git_view_0828.py'},
    'git-stack-0822.json': {'run_stack_audit.py':'run_stack_audit_0828.py'},
    'git-vendor-compile-0822.json': {'compile_vendor.py':'compile_vendor_0828.py'},
    'git-close-baseline-0823.json': {'run_git_view.py':'run_git_view_0828.py'},
    'app-panel-ui-0823.json': {'tools/pentest-elf-tests':'tools/app-panel-ui-tests-0828'},
    'consoles-ui-0823.json': {'tools/context-menu-tests':'tools/context-menu-tests-0828'},
    'media-0824.json': {'tools/media-tests':'tools/media-tests-0828'},
    'settings-recovery-0824.json': {'tools/system-migration-tests':'tools/system-migration-tests-0828'},
})
PREVIOUS_CONTRACT_PATHS = {
    name: {before: after.replace('0827','0828') for before, after in mapping.items()}
    for name, mapping in previous.PREVIOUS_CONTRACT_PATHS.items()
}


def execute_adapted(function, replacements, *args):
    source = inspect.getsource(function)
    for before, after in replacements.items():
        source = source.replace(before, after)
    namespace = dict(function.__globals__)
    exec(compile(source, '<unchanged prior assertions; explicit current identities>', 'exec'), namespace)
    return namespace[function.__name__](*args)


def checked_help(raw, release, sketch):
    return execute_adapted(older.checked_help, {'0.8.26':'0.8.28','0826':'0828'}, raw, release, sketch)


def checked_credential_extension(raw, release, sketch):
    return previous.checked_credential_extension(raw, release, sketch)


def checked_diagnostics(raw, release, sketch):
    return execute_adapted(previous.checked_diagnostics,
        {'tools/task-diagnostics-tests-0827':'tools/task-diagnostics-tests-0828',
         "== 968":"== 976", "== 4":"== 5", "== 7":"== 8",
         'PASS 1999 RTC diagnostic checks;':'PASS 2054 RTC diagnostic checks;',
         'corruptions=968;':'corruptions=976;'}, raw, release, sketch)


def checked_previous(name, raw, release, sketch, contracts=None):
    if raw.get('version') != '0.8.28':
        return previous.checked_previous(name, raw, release, sketch, contracts)
    mapping = {"'0.8.26'":"'0.8.28'", **PREVIOUS_CONTRACT_PATHS.get(name,{})}
    result = execute_adapted(older.checked_new, mapping, name, raw, release, sketch, contracts)
    if name == 'serial-backend-0826.json':
        match = re.search(r'PASS serial backend (\d+) checks', raw.get('output',''))
        require(match and int(match[1]) >= 588575, 'Serial ownership/whole-document checks incomplete')
    return result


def checked_previous_new(name, raw, release, sketch, contracts=None):
    if raw.get('version') != '0.8.28':
        return previous.checked_new(name, raw, release, sketch, contracts)
    if name == 'writing-serial-0827.json':
        return checked_writing_serial(raw,release,sketch)
    if name == 'serial-syntax-0827.json':
        from serial_target_contract_0828 import checked_serial_target
        return checked_serial_target(raw,release,sketch)
    mapping = {"'0.8.27'":"'0.8.28'"}
    for path in ('tools/serial-tests-0827','tools/vault-tests-0827','tools/writing-serial-tests-0827'):
        mapping[path] = path.replace('0827','0828')
    return execute_adapted(previous.checked_new, mapping, name, raw, release, sketch, contracts)


def checked_writing_serial(raw,release,sketch):
    """Retain complete editor coverage and bind the new .27 UI counterexample."""
    require(raw.get('version')=='0.8.28' and raw.get('passed') is True and
            raw.get('physicalDeviceTested') is False and raw.get('sourceStableDuringTests') is True,
            'Writing UI execution failed, changed sources or scope missing')
    hashes(sketch,raw.get('sources'),{'Tab5Writing.cpp','Tab5Writing.h','Tab5WritingSerial.inc',
           'Tab5Serial.h','Tab5SerialCore.h','Tab5SerialText.h','WriterDocument.h',
           'WriterRich.h','WriterView.h','WriterIndex.h','WriterDocument.cpp','WriterRich.cpp',
           'WriterView.cpp','WriterSyntax.cpp','WriterIndex.cpp','Tab5Focus.cpp',
           'Tab5TextInput.cpp','Tab5ContextMenu.cpp','Tab5UiStyle.cpp'})
    hashes(release/'tools/writing-serial-tests-0828',raw.get('tests'),{'run.py','test.cpp'})
    hashes(sketch/'sdcard/System/languages',raw.get('languages'),{locale+'.json' for locale in LANGUAGES})
    require(raw.get('lvglArchiveSha256')=='1a84414b953acdda6d2b5c49d94578d80099aca6e36d40b129c2f38c074b05a9',
            'Unexpected Writing LVGL archive')
    rows={row['mode']:row for row in raw.get('results',[])}
    require(set(rows)=={'baseline','current'} and all(row.get('exitCode')==0 for row in rows.values()),
            'Writing old-toolbar/current-menu executions missing')
    baseline=json.loads((release/'tools/verification/history-0827/evidence/source-manifest-final.json').read_text(encoding='utf8'))
    old={row['path']:row['sha256'] for row in baseline['files']}
    require(rows['baseline'].get('baselineSourceSha256')==old['Tab5Writing.cpp'] and
            'BASELINE .27 serial toolbar and long clear caption: 2 checks, expected regression reproduced' in rows['baseline'].get('output',''),
            'Missing counterexample from the delivered .27 editor')
    match=re.search(r'CURRENT writing serial: (\d+) checks',rows['current'].get('output',''))
    require(match and int(match[1])>=1872,'Writing selection/document/session/translated-menu executions incomplete')
    current=rows['current'].get('controllerHashes',{})
    required={'enum Action {','const char* tr(','uint32_t color(','writing::ViewTheme viewTheme(',
              'static void setLabel(','void caption(','const lv_font_t* font(','void panel(',
              'lv_obj_t* button(','lv_obj_t* label(','void build()','void layout()',
              'bool changedRich()','bool replaceSelection(','void closeMenu(','void toggleMenu(',
              'void Tab5WritingApp::show(','void Tab5WritingApp::resize(','serial','serialDispatch'}
    require(set(current)==required,'Writing production controller inventory incomplete')
    source=(sketch/'Tab5Writing.cpp').read_text(encoding='utf8')
    for signature,expected in current.items():
        if signature=='serial':value=(sketch/'Tab5WritingSerial.inc').read_text(encoding='utf8')
        elif signature=='serialDispatch':
            start=source.index('case SerialToggle:')
            value=source[start:source.index('\n',source.index('serialAction(action); break;',start))]
            start=source.index('case RefClose:')
            value+='\n'+source[start:source.index('case RefOutline:',start)]
        else:
            start=source.index(signature);value=source[start:closing(source,source.index('{',start))]
            if signature=='enum Action {':value+=';'
        require(hashlib.sha256(value.encode('utf8')).hexdigest()==expected,
                'Writing executed controller changed: '+signature)
    scoped(raw,'method',['production active Tab5Writing','boundary mocks','MSVC AddressSanitizer',
                        'LVGL archive is prebuilt and uninstrumented','actual translated captions'])


def checked_new(name, raw, release, sketch, contracts=None):
    require(name in NEW_REPORTS and raw.get('version') == '0.8.28'
            and raw.get('passed') is True and raw.get('physicalDeviceTested') is False,
            'Current execution missing or scope invalid: '+name)
    if name == 'compatibility-0828.json':
        hashes(release,raw.get('sourceHashes'),{sketch.name+'/'+n for n in
               ('Tab5Compatibility.cpp','Tab5Compatibility.h','Tab5CompatibilityPolicy.h','Tab5Audio.cpp','Tab5Audio.h')} |
               {'tools/compatibility-tests-0828/'+n for n in ('esp_chip_info.h','esp_event.h','esp_hosted.h',
                'freertos/FreeRTOS.h','freertos/portmacro.h','M5Unified.h','test.cpp','run.py')})
        require(raw.get('sourceStableDuringTests') is True and raw.get('exitCode')==0
                and raw.get('checks',0)>=57579,'Compatibility execution failed or source identity changed')
        require('PASS '+str(raw['checks'])+' compatibility checks;' in raw.get('output','') and
                'display_variants=3; concurrent_events=20000; cached_reads=10000; RPC=7' in raw['output'],
                'Compatibility display/transport/concurrent-event coverage incomplete')
        scoped(raw,'method',['Exact production compatibility policy and adapter','mocked I2C',
                            'MSVC AddressSanitizer','Audio hooks are target-compiled separately'])
        return
    if name == 'compatibility-target-0828.json':
        hashes(sketch,raw.get('sources'),{'Tab5Compatibility.cpp','Tab5Compatibility.h','Tab5CompatibilityPolicy.h','Tab5Audio.cpp','Tab5Audio.h'})
        hashes(release,raw.get('tests'),{'tools/compatibility-tests-0828/run_target.py'})
        rows=raw.get('results',[])
        require(len(rows)==2 and {row.get('name') for row in rows}=={'Tab5Compatibility.cpp','Tab5Audio.cpp'}
                and all(row.get('exitCode')==0 for row in rows),'Compatibility target/actual Audio hook compilation incomplete')
        require(raw.get('sourceStableDuringTests') is True and raw.get('hostedVersion')=='2.12.8'
                and raw.get('chipRevisionRange')==[1,199],'Compatibility SDK/version/silicon scope changed')
        scoped(raw,'method',['ESP32-P4 target syntax','ESP-Hosted 2.12.8','No mocked target headers'])
        return
    if name == 'battery-protection-0828.json':
        prefix=sketch.name+'/'
        hashes(release,raw.get('sourceHashes'),{prefix+'Tab5BatteryGuard.h',prefix+'Tab5BatteryGuard.cpp'} |
               {'tools/battery-protection-tests-0828/'+n for n in ('M5Unified.h','battery.cpp','run.py')})
        runs=raw.get('runs',[])
        require(len(runs)==19 and {row.get('scenario') for row in runs}=={'policy',*range(18)}
                and all(row.get('exitCode')==0 for row in runs),'Battery policy/adapter scenarios incomplete')
        counts=[re.search(r'PASS (\d+) checks',row.get('output','')) for row in runs]
        require(all(counts) and sum(int(match[1]) for match in counts)==raw.get('checks')
                and raw['checks']>=3313,'Battery executed assertion counts incomplete')
        scoped(raw,'method',['Exact production policy and hardware adapter','fake I2C/board/power-off','MSVC AddressSanitizer'])
        return
    if name == 'battery-ui-0828.json':
        hashes(release,raw.get('sourceHashes'),{sketch.name+'/Tab5BatteryUi.inc',sketch.name+'/Tab5BatteryGuard.h'} |
               {'tools/battery-protection-tests-0828/'+n for n in ('ui.cpp','run_ui.py')})
        require(raw.get('checks',0)>=1245 and 'PASS '+str(raw['checks'])+' checks' in raw.get('output',''),
                'Battery translated-layout/lifecycle/recovery-status executions incomplete')
        require(raw.get('lvglArchiveSha256')=='1a84414b953acdda6d2b5c49d94578d80099aca6e36d40b129c2f38c074b05a9',
                'Unexpected Battery UI LVGL archive')
        scoped(raw,'method',['Exact production BatteryUi include','real LVGL9.6','mocked sensor service',
                            'MSVC AddressSanitizer','LVGL archive uninstrumented'])
        return
    if name == 'battery-integration-0828.json':
        hashes(release,raw.get('sourceHashes'),{sketch.name+'/'+n for n in
               (sketch.name+'.ino','Tab5Tools.cpp','Tab5BatteryGuard.h','Tab5BatteryUi.inc')} |
               {'tools/battery-protection-tests-0828/'+n for n in ('firmware.cpp','run_integration.py')})
        require(raw.get('sourceStableDuringTests') is True and raw.get('checks',0)>=84
                and 'PASS '+str(raw['checks'])+' checks' in raw.get('output',''),
                'Battery firmware admission/restart/lifecycle integration execution incomplete')
        require(set(raw.get('controllerHashes',{}))=={'enabled','build','act','service'},
                'Battery firmware Tools controller inventory incomplete')
        source=(sketch/'Tab5Tools.cpp').read_text(encoding='utf8')
        for name,signature in {'enabled':'static void enabled(',
                'build':'if(which==Tab5Tool::Firmware){',
                'act':'if(tool==Tab5Tool::Firmware){\n   if((action',
                'service':'if(a->tool==Tab5Tool::Firmware){'}.items():
            begin=source.index(signature);value=source[begin:closing(source,source.index('{',begin))]
            require(hashlib.sha256(value.encode('utf8')).hexdigest()==raw['controllerHashes'][name],
                    'Battery firmware executed controller changed: '+name)
        require(raw.get('lvglArchiveSha256')=='1a84414b953acdda6d2b5c49d94578d80099aca6e36d40b129c2f38c074b05a9',
                'Unexpected firmware integration LVGL archive')
        scoped(raw,'method',['Exact Tools firmware construction/actions/service','real LVGL9.6',
                            'Main sketch call-order integration assertion','MSVC ASan','LVGL archive uninstrumented'])
        return
    if name == 'crypto-lock-0828.json':
        hashes(sketch,raw.get('sources'),{'Tab5CryptoLockCompat.c'})
        hashes(release,raw.get('tests'),{'tools/crypto-lock-tests-0828/'+n for n in ('run.py','main.cpp','sdk-original.c')})
        require(hashlib.sha256((release/'tools/crypto-lock-tests-0828/sdk-original.c').read_text(encoding='utf8').encode('utf8')).hexdigest()
                =='5ec2287139419de96af04c40afdff39829655d3b67893f2bdef8096070068c2b',
                'Crypto baseline differs from the exact upstream SDK lock source')
        require(raw.get('upstreamCommit')=='735507283d' and raw.get('upstreamFix')=='7fc2aaf',
                'Crypto compatibility upstream identity changed')
        rows={row['name']:row for row in raw.get('results',[])}
        require(set(rows)=={'baseline','patched','other-target','future-sdk'} and
                all(row.get('exitCode')==0 for row in rows.values()),'Crypto comparison or guarded SDK executions missing')
        for name,count,patched in [('baseline',102,0),('patched',103,1)]:
            output=rows[name].get('output','')
            match=re.search(r'overlap=(\d+)',output)
            require(('PASS '+str(count)+' crypto lock checks; patched='+str(patched)) in output
                    and 'controlled_rounds=33; concurrent_operations=8000;' in output and match,
                    'Crypto exclusion scenarios missing: '+name)
            require(int(match[1])==0 if patched else int(match[1])>0,
                    'Crypto baseline/current exclusion distinction missing: '+name)
        require(all(rows[n].get('output')=='SDK implementation retained' for n in ('other-target','future-sdk')),
                'Crypto compatibility guard executions incomplete')
        scoped(raw,'method',['Real complete SDK lock module','not a hardware reset','capability boundaries are simulated'])
        return
    if name == 'crypto-lock-target-0828.json':
        hashes(sketch,raw.get('sources'),{'Tab5CryptoLockCompat.c'})
        hashes(release,raw.get('tests'),{'tools/crypto-lock-tests-0828/run_target.py'})
        symbols={'esp_crypto_'+name+'_lock_'+op for name in ('ds','ecc','ecdsa','hmac','key_manager','mpi','sha_aes') for op in ('acquire','release')}
        require(raw.get('compileExit')==0 and raw.get('linkExit')==0 and raw.get('symbolCount')==14 and
                set(raw.get('symbols',[]))==symbols and raw.get('sdkOriginalModuleExtracted') is False,
                'Crypto target symbol parity/relocatable link verification incomplete')
        require(set(raw.get('stackFrames',{}))==symbols and all(type(v) is int and 0<=v<=16 for v in raw['stackFrames'].values()),
                'Crypto own-frame target measurements missing or changed')
        require(raw.get('sdkHashes')=={
            'lib/libesp_security.a':'63339734295d9fa355b78591fed7f46752b60748d15a16c938d1622afc62bee9',
            'versions.txt':'663d9b24ca5bd6f8770d06b5e2e485224b1b6ce5aa785be14892c914f120f3d5',
            'sdkconfig':'c5cdfe900db5bfa0159334dd67e767803e9c7ca1f8e0bf20df4ffba0c9c23bae'},
                'Unexpected target SDK crypto archive or configuration')
        scoped(raw,'method',['Real P4 C compile','symbol parity','Final firmware provenance requires','Own stack frames exclude callees'])
        return
    if name == 'crypto-lock-link-0828.json':
        hashes(sketch,raw.get('sources'),{'Tab5CryptoLockCompat.c'})
        hashes(release,raw.get('tests'),{'tools/crypto-lock-tests-0828/verify_final_link.py'})
        required={'esp_crypto_'+unit+'_lock_'+action for unit in ('ecdsa','ecc','sha_aes','mpi') for action in ('acquire','release')}
        defined=raw.get('definedSymbols',[])
        require(set(raw.get('requiredSymbols',[]))==required and required<=set(defined)
                and len(raw['requiredSymbols'])==len(required) and len(defined)==len(set(defined))
                and raw.get('sdkOriginalModuleExtracted') is False,
                'Final firmware crypto symbols are not uniquely owned by the compatibility object')
        original=(sketch/'Tab5CryptoLockCompat.c').read_bytes()
        mode=raw.get('sourceCopyMode')
        require(raw.get('sourceBodyIdentical') is True and mode in {'byte-identical','arduino-line-directive'},
                'Final firmware crypto source body was not verified byte-identical')
        directive=b''
        if mode=='arduino-line-directive':
            escaped_path=str((sketch/'Tab5CryptoLockCompat.c').resolve()).replace('\\','\\\\').replace('"','\\"')
            directive=('#line 1 "'+escaped_path+'"\n').encode('utf-8')
        require(raw.get('sourceCopyIdentical') is (mode=='byte-identical')
                and type(raw.get('arduinoLineDirectiveBytes')) is int
                and raw['arduinoLineDirectiveBytes']==len(directive)
                and raw.get('compiledSourceSha256')==hashlib.sha256(directive+original).hexdigest(),
                'Final firmware crypto build source differs beyond the exact Arduino source-location directive')
        controls=raw.get('sourceCopyNegativeControls',[])
        require(len(controls)==5 and set(controls)=={'modified-body','different-path','different-line','extra-directive','extra-newline'},
                'Final firmware crypto source-copy negative controls incomplete')
        hashes(release/'firmware/debug',raw.get('artifacts'),{sketch.name+'.ino.elf',sketch.name+'.ino.map'})
        addresses=raw.get('symbolAddresses',{})
        require(set(addresses)==required and all(isinstance(value,str) and re.fullmatch(r'0x[0-9a-f]+',value)
                and int(value,16)>0 for value in addresses.values()),'Final firmware crypto ELF addresses incomplete')
        require(len({int(value,16) for value in addresses.values()})==len(required),
                'Final firmware crypto lock addresses are not distinct')
        link_map=(release/'firmware/debug'/(sketch.name+'.ino.map')).read_text(encoding='utf-8',errors='replace')
        require('esp_crypto_lock.c.obj' not in link_map,'Original SDK crypto lock object present in delivered linker map')
        for symbol,address in addresses.items():
            pattern=r'\.text\.'+re.escape(symbol)+r'\s+(0x[0-9a-f]+)\s+0x[0-9a-f]+[^\n]*Tab5CryptoLockCompat\.c\.o'
            owners=[int(value,16) for value in re.findall(pattern,link_map,re.IGNORECASE)]
            require(owners.count(int(address,16))==1,
                    'Final crypto ELF address has no unique compatibility owner in delivered linker map: '+symbol)
        scoped(raw,'method',['Final ELF nm and GNU linker map','original SDK lock object absent',
                            'ELF addresses matching their sections owned by Tab5CryptoLockCompat.c.o',
                            'only one initial #line 1 directive naming the exact source path',
                            'byte-identical body','No other normalization accepted'])
        return
    if name == 'writing-recovery-0828.json':
        require(raw.get('sourceStableDuringTests') is True and raw.get('exitCode')==0,
                'Writing recovery execution failed or production sources changed')
        hashes(sketch,raw.get('sources'),{'Tab5Writing.cpp','Tab5Writing.h','WriterStorage.cpp',
               'WriterStorage.h','WriterDocument.cpp','WriterDocument.h'})
        hashes(release/'tools/writing-recovery-tests-0828',raw.get('tests'),{'run.py','test.cpp'})
        require(raw.get('extractorSha256')==digest(release/'tools/writing-serial-tests-0828/run.py'),
                'Recovery controller extractor changed after execution')
        match=re.search(r'Writing recovery: (\d+) checks',raw.get('output',''))
        require(match and int(match[1])>=1248,'Recovery copy/SD fault/rollback/deadline scenarios incomplete')
        scoped(raw,'method',['Actual recovery controllers','production SaveJob/Document','MSVC AddressSanitizer','filesystem faults'])
        return
    if name == 'git-target-0828.json':
        hashes(sketch,raw.get('sources'),{'Tab5Git.cpp','Tab5Git.h','Tab5GitPort.cpp','Tab5GitPort.h',
               'Tab5GitView.cpp','Tab5GitView.h','Tab5Focus.cpp','Tab5Focus.h'})
        hashes(release/'tools/git-tests',raw.get('tests'),{'run_target_0828.py'})
        rows=raw.get('results',[])
        require(len(rows)==4 and {row.get('source') for row in rows}==
                {'Tab5Git.cpp','Tab5GitPort.cpp','Tab5GitView.cpp','Tab5Focus.cpp'},'Git target units incomplete')
        require(all(row.get('exitCode')==0 and row.get('frames') and '-fstack-usage' in row.get('arguments',[])
                for row in rows),'Git real target compilation or static frame data missing')
        require(raw.get('wholeStackBound') is False,'Git frame measurements claimed as whole-stack bound')
        scoped(raw,'method',['Actual ESP32-P4','not a total call-chain bound'])
        return
    if name == 'vault-buffers-0828.json':
        require(raw.get('addressSanitizer') is True, 'Vault buffer comparison was not instrumented with ASan')
        hashes(release/'tools/vault-tests-0828', raw.get('testSources'), {'buffers.cpp','run_buffers.py'})
        rows={row['version']:row for row in raw.get('rows',[])}
        require(set(rows)=={'0.8.27','0.8.28'}, 'Vault buffer delivered-baseline/current comparison missing')
        baseline=json.loads((release/'tools/verification/history-0827/evidence/source-manifest-final.json').read_text(encoding='utf8'))
        old={row['path']:row['sha256'] for row in baseline['files']}
        required={'Tab5VaultCore.cpp','Tab5VaultCore.h'}
        require(set(rows['0.8.27'].get('sources',{}))==required and
                all(old.get(n)==v for n,v in rows['0.8.27']['sources'].items()),
                'Vault buffer baseline differs from the delivered .27 sources')
        hashes(sketch,rows['0.8.28'].get('sources'),required)
        for version,exit_code,failures in [('0.8.27',1,8194),('0.8.28',0,0)]:
            row=rows[version];obs=row.get('observations',{})
            require(row.get('exitCode')==exit_code and obs.get('checks',0)>=16435 and
                    obs.get('alignmentFailures')==failures and obs.get('wipedAllocations',0)>=8238 and
                    obs.get('wipeViolations')==0,'Vault layout/wipe/OOM/tamper comparison incomplete: '+version)
        scoped(raw,'method',['Actual VaultCore ARDUINO allocation branch','actual native mbedTLS3.6.5',
                            'not claimed as the hardware reset cause'])
        return
    raise ValueError('Unknown current execution contract: '+name)
