#!/usr/bin/env python3
"""Compile the real network gate; adapt only FreeRTOS synchronization/clock/serial to host threads."""
from pathlib import Path
import argparse,hashlib,json,subprocess,zipfile,re,os
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path,required=True);p.add_argument('--build',type=Path,required=True);a=p.parse_args();here=Path(__file__).resolve().parent;r=here.parent.parent;s=next(r.glob('Tab5OS_Arduino_v*_Ecriture'));b=a.build.resolve();b.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
sources={str((s/n).relative_to(r)):sha(s/n) for n in ('Tab5NetworkGate.cpp','Tab5NetworkGate.h')}
proof=r/'tools/verification/network-gate-regression-0848';proof.mkdir(exist_ok=True)
def run(name,src,current):
 exe=b/name;cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-g','-pthread','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(here),'-I'+str(src),str(here/'gate.cpp'),str(src/'Tab5NetworkGate.cpp'),'-o',str(exe)]
 if current:cmd.insert(1,'-DCURRENT_GATE')
 q=subprocess.run(cmd,capture_output=True,text=True);(proof/(name+'-build.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stdout+q.stderr
 q=subprocess.run([str(exe)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'),timeout=60);(proof/(name+'-run.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stdout+q.stderr;print(name,q.stdout.strip());return {'passed':True,'checks':int(re.search(r'checks=(\d+)',q.stdout)[1]),'output':q.stdout.strip()}
old=b/'baseline-source';old.mkdir(exist_ok=True)
with zipfile.ZipFile(a.baseline_zip) as z:
 for n in ('Tab5NetworkGate.cpp','Tab5NetworkGate.h','Tab5Serial.h','Tab5SerialCore.h'):
  path=next(x for x in z.namelist() if x.endswith('/'+n) and '/Tab5OS_Arduino_' in x and '/tools/' not in x);(old/n).write_bytes(z.read(path))
before=run('baseline',old,True);after=run('current',s,True);assert sources=={str((s/n).relative_to(r)):sha(s/n) for n in ('Tab5NetworkGate.cpp','Tab5NetworkGate.h')}
report={'version':'0.8.48','baselineVersion':'0.8.47','passed':True,'physicalDeviceTested':False,'baselineArchiveSha256':sha(a.baseline_zip),'baseline':before,'current':after,'sourceHashes':sources,'testSourceHashes':{str(p.relative_to(r)):sha(p) for p in here.rglob('*') if p.is_file()},'evidenceHashes':{str(p.relative_to(r)):sha(p) for p in proof.iterdir() if p.is_file()},'limits':['Actual production gate compiled, FreeRTOS mutex/event-group/clock and serial adapted to host threads. ASan/UBSan active; LSan disabled.','No real socket, ESP32 scheduling, hardware latency, RF throughput or radio audio quality measured.','.46 and .47 both retain established-radio sharing; no new admission improvement asserted. 100 radio reuse cycles/800 serialized concurrent transactions; mutex ownership assertions detect unintended unlocks.']};(proof.parent/'network-gate-regression-0848.json').write_text(json.dumps(report,indent=2)+'\n')
