#!/usr/bin/env python3
"""Production core + regression/viewport tests; host GCC with ASan and UBSan."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,default=Path('/tmp/tab5-image-core-0834'));a=p.parse_args()
root=Path(__file__).resolve().parents[2];tests=Path(__file__).resolve().parent
sketch=root/'Tab5OS_Arduino_v0.8.34_Ecriture';core=sketch/'tools/native-apps/image-editor';a.build.mkdir(parents=True,exist_ok=True)
flags=['-std=c11','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def run(name,file,source=core):
 cmd=['cc',*flags,'-I',str(source),str(tests/file),str(source/'image_core.c'),'-lm','-o',str(a.build/name)]
 subprocess.run(cmd,check=True);result=subprocess.run([str(a.build/name)],env=env,text=True,capture_output=True)
 (a.build/(name+'.log')).write_text(result.stdout+result.stderr)
 if result.returncode: print(result.stdout,result.stderr);raise SystemExit(result.returncode)
 data=json.loads(result.stdout.strip().splitlines()[-1]);print(name,data);return data,cmd
baseline,_=run('opacity-baseline','opacity_baseline.c',tests/'baseline-0833');corrected,_=run('opacity-corrected','opacity_baseline.c')
assert baseline['alpha']==254 and corrected['alpha']==127
files=[core/'image_core.c',core/'image_core.h',core/'test_core.c',*tests.glob('*.py'),*tests.glob('*.c'),* (tests/'baseline-0833').glob('*')]
hashes={str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
proof=root/'tools/verification';proof.mkdir(exist_ok=True)
for name in ('core','render'):
 data,cmd=run(name,name+'.c');report={'version':'0.8.34','passed':True,'physicalDeviceTested':False,'sourceHashes':hashes,'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'flags':flags,'result':data,'command':cmd,'limitations':['Host C core only; no Tab5 display, SD, HID or physical frame-rate measurement.','LeakSanitizer is disabled; tracked allocator must return to zero at cleanup.']}
 if name=='core': report['opacityRegression']={'baseline':baseline,'corrected':corrected,'exactShippedBaseline':'0.8.33'}
 (proof/('image-'+name+'-0834.json')).write_text(json.dumps(report,indent=2)+'\n')
assert all(hashlib.sha256((root/f).read_bytes()).hexdigest()==h for f,h in hashes.items())
