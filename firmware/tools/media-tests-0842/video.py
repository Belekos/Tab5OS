#!/usr/bin/env python3
"""Check exact production lifecycle methods with controlled worker/I/O boundaries."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.42_Ecriture';here=Path(__file__).resolve().parent
source=(sketch/'Tab5Video.cpp').read_text()
def block(text,start):
 pos=text.index(start);brace=text.index('{',pos);depth=0
 for end in range(brace,len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if not depth:return text[pos:end+1]
 raise AssertionError(start)
names=['bool Tab5VideoEngine::playSource(', 'void Tab5VideoEngine::stop(', 'void Tab5VideoEngine::service(', 'void Tab5VideoEngine::detachDisplay(', 'void Tab5VideoEngine::releaseFrames(', 'bool Tab5VideoEngine::snapshot(', 'bool Tab5VideoEngine::frame(', 'void Tab5VideoEngine::setState(', 'void Tab5VideoEngine::taskEntry(']
methods='\n\n'.join(block(source,n) for n in names)
cleanup=block(source,'void Tab5VideoEngine::taskMain(');cleanup=cleanup[cleanup.index('  tab5DiagnosticMark(4,"video-release"'):].rsplit('}',1)[0]
fixture=(here/'video_fixture.cpp').read_text().replace('// PRODUCTION_GUARD',block(source,'class MutexGuard')+';').replace('// PRODUCTION_METHODS',methods).replace('// PRODUCTION_CLEANUP',cleanup)
build=Path(tempfile.mkdtemp(prefix='tab5-video-lifecycle-'))
for n in ['Arduino.h','freertos/FreeRTOS.h','freertos/semphr.h','freertos/task.h']:
 p=build/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#pragma once\n')
(build/'Tab5Video.h').write_bytes((sketch/'Tab5Video.h').read_bytes());(build/'test.cpp').write_text(fixture)
cmd=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-I'+str(build),str(build/'test.cpp'),'-o',str(build/'test')]
subprocess.run(cmd,check=True)
run=subprocess.run([str(build/'test')],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));print(run.stdout+run.stderr,end='');run.check_returncode()
report={'version':'0.8.42',**json.loads(run.stdout.strip()),'scope':'Exact production playSource/stop/service/detachDisplay/releaseFrames/snapshot/frame/setState/taskEntry, actual class declaration, and actual taskMain cleanup tail. Threaded host worker body and SDK boundaries simulated; ASan+UBSan. No physical decoder, network or LVGL execution.','sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5Video.cpp',sketch/'Tab5Video.h',here/'video.py',here/'video_fixture.cpp']}}
out=root/'tools/audit-0842/video-lifecycle.json';out.write_text(json.dumps(report,indent=2)+'\n');print(out)
