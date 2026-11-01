#!/usr/bin/env python3
import argparse,hashlib,json,os,subprocess
from pathlib import Path

def block(text,prefix):
 start=text.index(prefix);depth=0
 for end in range(text.index('{',start),len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if depth==0:return text[start:end+1]
 raise ValueError(prefix)
p=argparse.ArgumentParser();p.add_argument('--baseline',required=True,type=Path);p.add_argument('--work',required=True,type=Path);a=p.parse_args();here=Path(__file__).resolve().parent;root=here.parents[1];sketch=root/'Tab5OS_Arduino_v0.8.48_Ecriture';a.work.mkdir(parents=True,exist_ok=True)
results={};hashes={}
for label,folder in [('before',a.baseline),('after',sketch)]:
 path=folder/'Tab5Audio.cpp';src=path.read_text();prefixes=['void Tab5AudioEngine::service()', 'void Tab5AudioEngine::serviceIdleMemoryPriority()', 'void Tab5AudioEngine::serviceWifiPower()', 'void Tab5AudioEngine::serviceWifiSignal()']
 if label=='after':prefixes.append('bool Tab5AudioEngine::wifiPowerTarget(')
 methods='\n\n'.join(block(src,prefix) for prefix in prefixes)
 hashes[label]={'sourceSha256':hashlib.sha256(path.read_bytes()).hexdigest(),'extractedMethodsSha256':hashlib.sha256(methods.encode()).hexdigest()}
 text=(here/'housekeeping_fixture.cpp').read_text().replace('// METHODS',methods).replace('// SIGNAL_REQUEST',block((sketch/'Tab5Audio.h').read_text(),'  void requestWifiSignal('));source=a.work/(label+'.cpp');source.write_text(text);binary=a.work/label
 c=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I'+str(sketch),' -DPATCHED='+str(int(label=='after')),str(source),'-o',str(binary)];c=[x.strip() for x in c]
 r=subprocess.run(c,capture_output=True,text=True);(a.work/(label+'-build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stderr)
 r.check_returncode();r=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(a.work/(label+'-run.log')).write_text(r.stdout+r.stderr);print(label,r.stdout,r.stderr);r.check_returncode();results[label]=json.loads(r.stdout)
report={'passed':True,'version':'0.8.48',**results,'comparativeSourceHashes':hashes,'sourceHashes':{str(q.relative_to(root)):hashlib.sha256(q.read_bytes()).hexdigest() for q in [sketch/'Tab5Audio.cpp',sketch/'Tab5Audio.h',sketch/'Tab5AudioPriority.h',sketch/'Tab5AudioWorkerLifecycle.h',Path(__file__),here/'housekeeping_fixture.cpp']},'fixtureSha256':hashlib.sha256((here/'housekeeping_fixture.cpp').read_bytes()).hexdigest(),'scope':'Actual service, serviceIdleMemoryPriority, serviceWifiSignal, serviceWifiPower and new wifiPowerTarget, with actual atomic lifecycle/priority classes. WiFi RPC, millis, queue/task creation and mutex boundaries controlled. Optional versus essential admission, reclaim/priority, connection/request events, paused radio, failed power RPC/retry, active worker, concurrent request during RSSI and rollover tested. No physical CPU/latency/heap result. ASan/UBSan enabled; LSan unavailable in sandbox.'}
(root/'tools/verification/audio-housekeeping-0848.json').write_text(json.dumps(report,indent=2)+'\n')
