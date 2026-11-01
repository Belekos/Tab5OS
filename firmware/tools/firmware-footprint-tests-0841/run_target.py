#!/usr/bin/env python3
"""Compile firmware footprint with the actual pinned P4 SDK; no link/hardware test."""
import argparse,hashlib,json,pathlib,subprocess
here=pathlib.Path(__file__).resolve().parent
root=here.parents[1]
sketch=root/'Tab5OS_Arduino_v0.8.41_Ecriture'
ap=argparse.ArgumentParser()
ap.add_argument('--build-root',type=pathlib.Path,default=root.parents[1]/'build_0841')
ap.add_argument('--sdk-image-source',type=pathlib.Path)
args=ap.parse_args()
packages=args.build_root/'data/packages/m5stack/tools'
sdk=packages/'esp32p4_es-libs/3.3.9'
toolchain=packages/'esp-rv32/2601/bin'
out=args.build_root/'footprint-target';out.mkdir(parents=True,exist_ok=True)
source=sketch/'Tab5FirmwareFootprint.cpp';obj=out/'Tab5FirmwareFootprint.cpp.o'
command=[str(toolchain/'riscv32-esp-elf-g++'),'@'+str(sdk/'flags/cpp_flags'),
         '@'+str(sdk/'flags/defines'),'-iprefix',str(sdk/'include')+'/',
         '@'+str(sdk/'flags/includes'),'-I'+str(sdk/'dio_qspi/include'),
         '-I'+str(sketch),'-DARDUINO_ARCH_ESP32','-Os','-fstack-usage','-c',str(source),'-o',str(obj)]
r=subprocess.run(command,text=True,capture_output=True)
(out/'compile.log').write_text(r.stdout+r.stderr)
if r.returncode: print(r.stdout+r.stderr);r.check_returncode()
nm=subprocess.run([str(toolchain/'riscv32-esp-elf-nm'),'-S',str(obj)],check=True,text=True,capture_output=True).stdout
(out/'symbols.log').write_text(nm)
undefined=[line.strip().split()[-1] for line in nm.splitlines() if ' U ' in line]
assert 'esp_image_get_metadata' in undefined and 'esp_partition_read' in undefined
assert 'esp_ota_get_running_partition' in undefined
assert not any(any(x in s.lower() for x in ['esp_image_verify','malloc','calloc','realloc','sha256','getsketchsize']) for s in undefined),undefined
sdk_review=None
if args.sdk_image_source:
    sdk_text=args.sdk_image_source.read_text()
    # Exact pinned SDK-source audit, not execution of the SDK parser on host.
    def body(name):
        import re
        m=re.search(r'\b'+name+r'\([^;]*?\)\s*\{',sdk_text,re.S)
        assert m,name
        start=m.end()-1;depth=0
        for end in range(start,len(sdk_text)):
            if sdk_text[end]=='{':depth+=1
            elif sdk_text[end]=='}':
                depth-=1
                if depth==0:return sdk_text[start:end+1]
        raise AssertionError(name)
    metadata=body('esp_image_get_metadata')
    segment=body('process_segment_data')
    checksum=body('process_checksum')
    assert 'bool do_verify = false' in metadata and 'bool do_load = false' in metadata
    assert 'process_segments(metadata, silent, do_load, NULL, NULL)' in metadata
    assert 'process_checksum(NULL, 0, metadata, silent, skip_check_checksum)' in metadata
    assert 'bool skip_check_checksum = true' in metadata
    assert segment.index('if (!do_load && checksum == NULL)') < segment.index('bootloader_mmap(')
    early=segment[:segment.index('bootloader_mmap(')]
    assert 'return ESP_OK;' in early
    assert 'if (!skip_check_checksum || sha_handle != NULL)' in checksum
    sdk_review={'source':'ESP-IDF v5.5.4 components/bootloader_support/src/esp_image_format.c',
                'sha256':hashlib.sha256(args.sdk_image_source.read_bytes()).hexdigest(),
                'mode':'Source audit only; not SDK host execution',
                'payloadMappingSkipped':True,'checksumReadSkipped':True}
report={'version':'0.8.41','passed':True,'physicalDeviceTested':False,
        'sdk':'M5Stack 3.3.9, ESP-IDF 5.5.4, esp-rv32 2601','command':command,
        'undefinedSymbols':undefined,'warnings':r.stderr,
        'stackUsage':obj.with_suffix('.su').read_text(),
        'sdkMetadataReview':sdk_review,
        'sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source,sketch/'Tab5FirmwareFootprint.h',here/'run_target.py']}}
report_path=root/'tools/verification/firmware-footprint-target-0841.json'
report_path.write_text(json.dumps(report,indent=2)+'\n')
print(report_path);print(nm)
