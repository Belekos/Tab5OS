#!/usr/bin/env python3
"""Test production snapshot with overlapping, capability-filtered heap regions."""
import hashlib,json,os,pathlib,subprocess,tempfile
here=pathlib.Path(__file__).resolve().parent
root=here.parents[1]
sketch=root/'Tab5OS_Arduino_v0.8.49_Ecriture'
with tempfile.TemporaryDirectory(prefix='tab5-memory-layout-0849-') as temporary:
    for p4 in (1,0):
        exe=pathlib.Path(temporary)/f'memory-layout-{p4}'
        command=['g++','-std=c++17','-g','-O1','-Wall','-Wextra','-Werror',
                 '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                 f'-DCONFIG_IDF_TARGET_ESP32P4={p4}',
                 '-I',str(here/'stubs'),'-I',str(sketch),
                 str(here/'main.cpp'),str(sketch/'Tab5MemoryLayout.cpp'),'-o',str(exe)]
        subprocess.run(command,check=True)
        subprocess.run([str(exe)],check=True,env=dict(os.environ,
            ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
inputs=[here/'run.py',here/'main.cpp',here/'stubs/sdkconfig.h',here/'stubs/esp_heap_caps.h',
        sketch/'Tab5MemoryLayout.h',sketch/'Tab5MemoryLayout.cpp']
out=root/'tools/verification/memory-layout-0849.json';out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps({'version':'0.8.49','passed':True,'physicalDeviceTested':False,
    'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],
    'scope':['Production capability-filtered snapshot','Overlapping INTERNAL/HP/TCM/LP queries','External DMA excluded from HP','Zero largest-block walks in default periodic sample','Largest block queried directly for fragmented HP','Unknown target exposes no invented physical split'],
    'sourceHashes':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}},indent=2)+'\n')
print(out)
