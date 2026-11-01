#!/usr/bin/env python3
"""Re-run production C6 compatibility and network arbitration against 0.8.42."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.42_Ecriture'
# Retain historical fixtures and reports. The adapter methods use the current
# sketch selected by the existing runner, with this release's report target.
runner=root/'tools/compatibility-tests-0832/run.py'
code=runner.read_text().replace('Tab5OS_Arduino_v0.8.32_Ecriture','Tab5OS_Arduino_v0.8.42_Ecriture').replace('"0.8.32"','"0.8.42"').replace('tools/verification/compatibility-0832.json','tools/audit-0842/compatibility.json')
try:
 exec(compile(code,str(runner),'exec'),{'__file__':str(runner),'__name__':'__main__'})
except SystemExit as result:
 if result.code:raise
report=root/'tools/audit-0842/compatibility.json';data=json.loads(report.read_text());assert data['passed'];data['runnerSha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();report.write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n')
legacy=root/'tools/network-gate-tests-0826';build=Path(tempfile.mkdtemp(prefix='tab5-gate-0842-'))
command=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-I'+str(legacy),'-I'+str(sketch),str(legacy/'gate.cpp'),str(sketch/'Tab5NetworkGate.cpp'),'-o',str(build/'gate')]
subprocess.run(command,check=True)
run=subprocess.run([str(build/'gate')],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));print(run.stdout+run.stderr,end='');run.check_returncode()
report={'version':'0.8.42','passed':True,'output':run.stdout,'scope':'Actual network gate + historical deterministic scheduler/semaphore fixture, ASan+UBSan. No real RTOS/network.','sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5NetworkGate.cpp',sketch/'Tab5NetworkGate.h',legacy/'gate.cpp',Path(__file__)]}}
(root/'tools/audit-0842/network-gate.json').write_text(json.dumps(report,indent=2)+'\n')
