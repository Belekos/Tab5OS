#!/usr/bin/env python3
"""Exercise extracted production audio worker methods with a threaded RTOS/driver fixture."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess, tempfile, zipfile
p=argparse.ArgumentParser();p.add_argument('--build');p.add_argument('--baseline-zip',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2]
sketch=next(root.glob('Tab5OS_Arduino_*_Ecriture')); here=Path(__file__).resolve().parent
build=Path(a.build).resolve() if a.build else Path(tempfile.mkdtemp(prefix='tab5-audio-worker-0844-'))
build.mkdir(parents=True,exist_ok=True)
def method(text,name):
 start=text.index(name); brace=text.index('{',start);depth=0
 for end in range(brace,len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if depth==0:return text[start:end+1]
 raise ValueError(name)
source=(sketch/'Tab5Audio.cpp').read_text()
if a.baseline_zip:
 with zipfile.ZipFile(a.baseline_zip) as z: source=z.read(next(n for n in z.namelist() if n.endswith('/Tab5Audio.cpp'))).decode()
names=['void Tab5AudioEngine::audioTaskEntry(', 'bool Tab5AudioEngine::canRetireAudioTask(',
 'size_t Tab5AudioEngine::reclaimIdleWorker(', 'bool Tab5AudioEngine::startAudioTask(',
 'void Tab5AudioEngine::service(', 'void Tab5AudioEngine::serviceIdleMemoryPriority(', 'uint32_t Tab5AudioEngine::acquireMemoryPriority(']
methods='\n\n'.join(method(source,n) for n in names)
fixture=(here/'lifecycle_fixture.cpp').read_text().replace('// TEST_EVENT_WAITS','#define TEST_EVENT_WAITS '+('0' if a.baseline_zip else '1')); (build/'test.cpp').write_text(fixture.replace('// PRODUCTION_METHODS',methods))
for name in ['Tab5Task.h','Tab5AudioPriority.h','Tab5AudioWorkerLifecycle.h']:
 shutil.copyfile(sketch/name,build/name)
for name in ['Tab5Serial.h','Tab5ResourceManager.h','Tab5Memory.h','Arduino.h','freertos/FreeRTOS.h','freertos/task.h']:
 path=build/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('#pragma once\n')
cmd=['g++','-std=c++17','-g','-O1','-pthread','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-pie','-no-pie','-DARDUINO','-I',str(build),str(build/'test.cpp'),'-o',str(build/'test')]
subprocess.run(cmd,check=True)
import os
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
run=subprocess.run([str(build/'test')],capture_output=True,text=True,env=env)
(build/'run.log').write_text(run.stdout+run.stderr)
print(run.stdout+run.stderr,end='')
if run.returncode:raise SystemExit(run.returncode)
data=json.loads(run.stdout.strip().splitlines()[-1])
files=[sketch/'Tab5Audio.cpp',sketch/'Tab5Audio.h',sketch/'Tab5Task.h',sketch/'Tab5AudioPriority.h',sketch/'Tab5AudioWorkerLifecycle.h',here/'lifecycle.py',here/'lifecycle_fixture.cpp']
report={'version':'0.8.46','passed':True,'physicalDeviceTested':False,'baselineArchiveSha256':hashlib.sha256(a.baseline_zip.read_bytes()).hexdigest() if a.baseline_zip else None,**data,'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],
 'scope':'Extracted production audioTaskEntry/canRetireAudioTask/reclaimIdleWorker/startAudioTask/service/serviceIdleMemoryPriority/acquireMemoryPriority and unchanged production task trampoline/priority control, using host threads and simulated heap/task/driver/network services.',
 'limitations':['No Tab5 hardware, C6 RPC, real decoder, real I2S, or FreeRTOS scheduler was executed. Host simulated stack accounting verifies admission and release order, not measured device SRAM gains.','LeakSanitizer disabled because the execution environment prevents reliable process inspection.'],
 'sourceHashes':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}}
out=(build/'audio-lifecycle-before.json') if a.baseline_zip else root/'tools/verification/audio-lifecycle-0846.json';out.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n'); print('Report:',out)
