#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parents[1];sketch=root/'Tab5OS_Arduino_v0.8.49_Ecriture'
def block(text,start):
 p=text.index(start);b=text.index('{',p);depth=0
 for e in range(b,len(text)):
  if text[e]=='{':depth+=1
  elif text[e]=='}':
   depth-=1
   if not depth:return text[p:e+1]
 raise ValueError(start)
source=(sketch/'Tab5Audio.cpp').read_text()
fixture=(here/'fixture.cpp').read_text().replace('// PRODUCTION_BUFFERING',block(source,'class Tab5PsramStream')+';\n'+block(source,'class Tab5BufferedRadioSource')+';').replace('// PRODUCTION_START',block(source,'bool Tab5AudioEngine::startRadioNow('))
build=Path(tempfile.mkdtemp(prefix='tab5-radio-lifecycle-0849-'));(build/'fixture.cpp').write_text(fixture)
command=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-pthread','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-I'+str(sketch),str(build/'fixture.cpp'),str(sketch/'Tab5ResourceManager.cpp'),'-o',str(build/'test')]
r=subprocess.run(command,text=True,capture_output=True);print(r.stderr);r.check_returncode()
r=subprocess.run([str(build/'test')],text=True,capture_output=True,timeout=45,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));print(r.stdout+r.stderr,end='');r.check_returncode()
report={'version':'0.8.49',**json.loads(r.stdout),'physicalDeviceTested':False,'scope':'Actual startRadioNow, radioBudget/full resource manager, Tab5BufferedRadioSource and Tab5PsramStream; RTOS task/stream/semaphore APIs mapped to native host threads. Codec/output/HTTP payload boundaries simulated. Cases MP3/AAC start, I2S retained transition, fragment-limited heap fallback to64KiB, admission rejection and upstream failure; ASan/UBSan and explicit allocation count. No radio sound or wireless throughput measured.','sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5Audio.cpp',sketch/'Tab5ResourceManager.cpp',sketch/'Tab5ResourceManager.h',sketch/'Tab5ResourcePolicy.h',here/'run.py',here/'fixture.cpp']}}
(root/'tools/verification/radio-lifecycle-0849.json').write_text(json.dumps(report,indent=2)+'\n')
