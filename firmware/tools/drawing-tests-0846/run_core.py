#!/usr/bin/env python3
"""Actual image core: authenticated .45 comparison, mixed history, poisoned allocations and OOM."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--build',required=True,type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[2];t=Path(__file__).parent;s=r/'Tab5OS_Arduino_v0.8.46_Ecriture/tools/native-apps/image-editor';b=a.build.resolve();b.mkdir(parents=True,exist_ok=True)
proof=r/'tools/verification/drawing-core-0846';proof.mkdir(exist_ok=True,parents=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inputs=[s/n for n in ['image_core.c','image_core.h','image_selection.h']]
inputs += [t/'gesture.c',Path(__file__),s/'test_core.c',r/'tools/drawing-tests-0841/core.c']
inputs += sorted((t/'baseline').glob('*'))
before={str(p.relative_to(r)):sha(p) for p in inputs}
flags=['-std=c11','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def run(cmd,name):
 c=subprocess.run([str(x) for x in cmd],env=env,capture_output=True,text=True,timeout=120)
 (proof/(name+'.log')).write_text(c.stdout+c.stderr)
 if c.returncode:print(c.stdout+c.stderr);raise SystemExit(c.returncode)
 return c.stdout
manifest=json.loads((t/'baseline/manifest.json').read_text())
for name,digest in manifest['sourceSha256'].items():assert sha(t/'baseline'/name)==digest
results={}
for label,src,arg in [('baseline',t/'baseline',['--baseline']),('current',s,[])]:
 exe=b/label
 run(['cc',*flags,'-fno-builtin-memcpy','-fno-builtin-memset','-I',src,t/'gesture.c',src/'image_core.c','-Wl,--wrap=memcpy','-Wl,--wrap=memset','-o',exe],label+'-build')
 results[label]=run([exe,*arg],label);print(label+': '+results[label])
assert results['baseline'].splitlines()[0]==results['current'].splitlines()[0], 'Mixed history has changed observable pixels, layer metadata or selection'
for label,test in [('legacy-core',s/'test_core.c'),('signed-selection',r/'tools/drawing-tests-0841/core.c')]:
 exe=b/label;run(['cc',*flags,'-I',s,test,s/'image_core.c','-o',exe],label+'-build');results[label]=run([exe],label);print(results[label])
assert before=={str(p.relative_to(r)):sha(p) for p in inputs}
report={'version':'0.8.46','passed':True,'physicalDeviceTested':False,'method':__doc__,'sourceHashes':{k:v for k,v in before.items() if k.startswith('Tab5OS_')},'testSourceHashes':{k:v for k,v in before.items() if not k.startswith('Tab5OS_')},'baseline':manifest,'flags':flags,'results':results,'sanitizers':['address','undefined'],'evidenceHashes':{str(p.relative_to(r)):sha(p) for p in sorted(proof.glob('*.log'))},'limitations':['Host execution; no device latency or PSRAM bandwidth measured.','Allocator uses poisoned host RAM; tracks all outstanding allocations. LSan disabled, independent allocator reaches zero.','Memcpy/memset byte accounting instruments C library calls; it is not a bus-traffic or CPU measure.','Full generic snapshots and allocator reclamation policy retained.']}
(proof.parent/'drawing-core-0846.json').write_text(json.dumps(report,indent=2)+'\n')
