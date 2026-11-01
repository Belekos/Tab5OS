#!/usr/bin/env python3
"""Execute production core, .47 differential oracle, sparse-undo poisoned-memory tests."""
from pathlib import Path
import argparse,subprocess,hashlib,json,os
p=argparse.ArgumentParser();p.add_argument('--build',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parents[1];src=release/'Tab5OS_Arduino_v0.8.48_Ecriture/tools/native-apps/image-editor';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True);proof=release/'tools/verification/drawing-0848';proof.mkdir(parents=True,exist_ok=True)
flags=['-std=c11','-Wall','-Wextra','-Werror','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-fno-builtin-memcpy','-fno-builtin-memset']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inputs=[p for p in here.rglob('*') if p.is_file()]+[src/n for n in ('image_core.c','image_core.h','image_selection.h','test_core.c')]+[release/'tools/drawing-tests-0841/core.c']
before={str(p.relative_to(release)):sha(p) for p in inputs}
def run(cmd,name):
 r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,env=env,timeout=180);(proof/(name+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
 return r.stdout
results={}
for label,source in [('baseline',here/'baseline'),('current',src)]:
 for fixture in ('gesture','differential'):
  binary=work/(label+'-'+fixture);run(['cc',*flags,'-I',source,here/(fixture+'.c'),source/'image_core.c','-Wl,--wrap=memcpy','-Wl,--wrap=memset','-o',binary],label+'-'+fixture+'-build');results[label+'-'+fixture]=run([binary],label+'-'+fixture)
assert results['baseline-gesture'].splitlines()[0]==results['current-gesture'].splitlines()[0]
assert results['baseline-differential']==results['current-differential'],'Observable document regression'
for name,fixture,extra in [('poison',here/'poison.c',['-Wl,--wrap=memcpy']),('core',src/'test_core.c',[]),('selection',release/'tools/drawing-tests-0841/core.c',[])]:
 binary=work/name;run(['cc',*flags,'-I',src,fixture,src/'image_core.c',*extra,'-o',binary],name+'-build');results[name]=run([binary],name)
assert before=={str(p.relative_to(release)):sha(p) for p in inputs}
report={'version':'0.8.48','passed':True,'physicalDeviceTested':False,'method':__doc__,'flags':flags,'sourceHashes':before,'differential':{'operations':4000,'traceSha256':hashlib.sha256(results['current-differential'].encode()).hexdigest(),'identical':True},'results':{k:v for k,v in results.items() if 'differential' not in k},'evidenceHashes':{str(p.relative_to(release)):sha(p) for p in proof.glob('*.log')},'limitations':['Host core execution, not physical PSRAM bandwidth or latency.','Sparse pixel allocations remain reserved at full worst-case size to prevent mid-gesture allocation failure.','Selection masks still copied completely; full-layer operations keep their established snapshot path.','Poison tests unpoison destination ranges only upon first memcpy write, so any uncaptured read fails ASan.']}
(proof.parent/'drawing-core-0848.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k in ('passed','differential','results')},indent=2))
