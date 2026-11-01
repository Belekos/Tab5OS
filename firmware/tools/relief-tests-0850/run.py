#!/usr/bin/env python3
"""Actual C editor core and contour fill, with bounded host allocator and fault injection."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[1];core=root/'Tab5OS_Arduino_v0.8.50_Ecriture/tools/native-apps/image-editor';a.build.mkdir(parents=True,exist_ok=True)
files=[core/'image_core.c',core/'image_core.h',core/'image_selection.h',here/'test.c',here/'run.py']
def hashes():return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
before=hashes();flags=['-std=c11','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
cmd=['cc',*flags,'-I'+str(core),str(here/'test.c'),'-o',str(a.build/'test')];r=subprocess.run(cmd,text=True,capture_output=True);(a.build/'compile.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');r=subprocess.run([str(a.build/'test'),str(a.build/'preview.ppm')],text=True,capture_output=True,env=env);(a.build/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
assert before==hashes(),'Sources changed during test; rerun after freeze'
result=json.loads(r.stdout.strip().splitlines()[-1]);proof=root/'tools/verification';proof.mkdir(exist_ok=True)
from PIL import Image
Image.open(a.build/'preview.ppm').save(proof/'relief-0850-preview.png')
report={'version':'0.8.50','passed':True,'physicalDeviceTested':False,'method':__doc__,'sanitizers':['ASan','UBSan'],'sourceHashes':before,'result':result,'limits':['Host core and bounded allocator, no physical latency, LVGL or PSRAM measurements.','LeakSanitizer disabled; every case checks zero tracked bytes after destruction.','2D rounded raster relief; chamfer distance approximates Euclidean contour distance.']}
(proof/'relief-0850.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
