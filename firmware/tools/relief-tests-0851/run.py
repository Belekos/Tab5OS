#!/usr/bin/env python3
"""Actual C editor core and contour fill, with bounded host allocator and fault injection."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[1];core=root/'Tab5OS_Arduino_v0.8.51_Ecriture/tools/native-apps/image-editor';a.build.mkdir(parents=True,exist_ok=True)
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
from PIL import ImageDraw
render=Image.open(a.build/'preview.ppm'); sheet=Image.new('RGB',(600,450),'white');sheet.paste(render,(0,25));draw=ImageDraw.Draw(sheet)
for x,title in [(10,'Linear / cone'),(210,'Circular / sphere'),(410,'Extrusion')]:draw.text((x,6),title,fill='black')
draw.text((10,430),'Top: +45 px    Bottom: -45 px    Extent 100%    Light 225 deg / 100%',fill='black');sheet.save(proof/'relief-0851-preview.png')
report={'version':'0.8.51','passed':True,'physicalDeviceTested':False,'method':__doc__,'sanitizers':['ASan','UBSan'],'sourceHashes':before,'result':result,'evidenceHashes':{'tools/verification/relief-0851-preview.png':hashlib.sha256((proof/'relief-0851-preview.png').read_bytes()).hexdigest()},'limits':['Host core and bounded allocator, no physical latency, LVGL or PSRAM measurements.','LeakSanitizer disabled; every case checks zero tracked bytes after destruction.','2D raster lighting; exact distance to zero raster samples, Q4quantization. Not a 3Dmesh or localmedialaxis extrusion.']}
(proof/'relief-0851.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
