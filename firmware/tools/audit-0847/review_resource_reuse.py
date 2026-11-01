#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,os,shutil
r=Path(__file__).resolve().parents[2];s=r/'Tab5OS_Arduino_v0.8.47_Ecriture';w=r.parent.parent
b=w/'build_0847/resource-independent-review';b.mkdir(parents=True,exist_ok=True)
stubs=r/'tools/memory-tests-0847/stubs';shutil.copytree(stubs,b/'stubs',dirs_exist_ok=True)
p=b/'stubs/freertos/event_groups.h';text=p.read_text().replace('extern std::function<void()> beforeEventWait;','extern std::function<void()> beforeEventWait,beforeEventSet;').replace('++eventSets;std::lock_guard','++eventSets;if(beforeEventSet)beforeEventSet();std::lock_guard');p.write_text(text)
source=s/'Tab5ResourceManager.cpp';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before=sha(source)
(b/'core.cpp').write_text(source.read_text().split('\n#ifdef ARDUINO\n#include "Tab5Memory.h"')[0])
cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-pthread','-DARDUINO','-I'+str(b/'stubs'),'-I'+str(s),str(Path(__file__).with_suffix('.cpp')),str(b/'core.cpp'),'-o',str(b/'test')]
cp=subprocess.run(cmd,capture_output=True,text=True);(b/'build.log').write_text(cp.stdout+cp.stderr)
if cp.returncode:print(cp.stderr);raise SystemExit(cp.returncode)
cp=subprocess.run([str(b/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(b/'run.log').write_text(cp.stdout+cp.stderr);print(cp.stdout+cp.stderr)
if cp.returncode:raise SystemExit(cp.returncode)
assert before==sha(source),'Production source changed during review test'
report={'version':'0.8.47','passed':True,'physicalDeviceTested':False,'method':'Independent production Manager core, ARDUINO path, controlled RTOS boundary pauses only native event publication after Done; 100 cycles force same slot reuse and stale event delivery.','result':cp.stdout.strip(),'sourceHashes':{str(source.relative_to(r)):before,'Tab5OS_Arduino_v0.8.47_Ecriture/Tab5ResourceManager.h':sha(s/'Tab5ResourceManager.h')},'limits':['No target timing or priority inheritance measurement','Native RTOS APIs adapted using host mutex/condition_variable','ASan/UBSan active, LSan disabled']}
v=r/'tools/verification';(v/'resource-independent-0847.json').write_text(json.dumps(report,indent=2)+'\n')
out=v/'resource-independent-0847';out.mkdir(exist_ok=True)
for name in ['build.log','run.log']:shutil.copyfile(b/name,out/name)
