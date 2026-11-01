#!/usr/bin/env python3
"""Production caching/error-path regression with mocked ESP-IDF boundaries."""
import hashlib,json,os,pathlib,subprocess,tempfile
here=pathlib.Path(__file__).resolve().parent
root=here.parents[1]
sketch=root/'Tab5OS_Arduino_v0.8.41_Ecriture'
inputs=[here/'run.py',here/'main.cpp',*sorted((here/'stubs').glob('*.h')),
        sketch/'Tab5FirmwareFootprint.h',sketch/'Tab5FirmwareFootprint.cpp']
results=[]
with tempfile.TemporaryDirectory(prefix='tab5-footprint-0841-') as d:
    exe=pathlib.Path(d)/'firmware-footprint'
    command=['g++','-std=c++17','-g','-O1','-Wall','-Wextra','-Werror',
             '-fsanitize=address,undefined','-fno-omit-frame-pointer',
             '-I',str(here/'stubs'),'-I',str(sketch),
             str(here/'main.cpp'),str(sketch/'Tab5FirmwareFootprint.cpp'),'-o',str(exe)]
    subprocess.run(command,check=True)
    for scenario in range(15):
        r=subprocess.run([str(exe),str(scenario)],check=True,text=True,capture_output=True,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
                     UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
        results.append(r.stdout.strip());print(r.stdout,end='')
out=root/'tools/verification/firmware-footprint-0841.json'
out.write_text(json.dumps({'version':'0.8.41','passed':True,'physicalDeviceTested':False,
    'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],
    'scenarios':15,'cachedAccesses':1500000,
    'scope':['Production firmware-size helper with mocked SDK boundaries',
             'Bounds/header validation and SDK error propagation',
             'Exactly one running-partition lookup per boot',
             'No further flash I/O after first read, including errors'],
    'limitations':['Metadata SDK implementation is not executed by host mocks',
                   'LeakSanitizer disabled: sandbox cannot inspect process tasks',
                   'Integrity verification remains the responsibility of boot/OTA'],
    'results':results,
    'sourceHashes':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}},indent=2)+'\n')
print(out)
