#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,subprocess,shutil,os
p=argparse.ArgumentParser();p.add_argument('--lvgl',type=Path,required=True);p.add_argument('--lvgl-build',type=Path,required=True);p.add_argument('--build',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parent.parent;sk=root/'Tab5OS_Arduino_v0.8.50_Ecriture';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True)
sources=['Tab5Alarm.cpp','Tab5Alarm.h','Tab5AlarmPolicy.h','Tab5AlarmTone.h','Tab5UiStyle.cpp','Tab5UiStyle.h','Tab5Icons.h','Tab5HomeUi.h','Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h']
def hashes():return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*[sk/s for s in sources],*[p for p in here.rglob('*') if p.is_file() and '__pycache__' not in p.parts]]}
before=hashes()
for src in (here/'stubs').iterdir():shutil.copyfile(src,work/src.name)
shutil.copyfile(sk/'Tab5Alarm.cpp',work/'Tab5Alarm.cpp')
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(work),'-I'+str(sk),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include'),'-I'+str(a.lvgl.resolve()/'src')]
cmd=['g++',*flags,str(here/'test.cpp'),*[str(sk/s) for s in ['Tab5UiStyle.cpp','Tab5Focus.cpp','Tab5TextInput.cpp','WriterDocument.cpp']],str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(work/'test')]
r=subprocess.run(cmd,capture_output=True,text=True);(work/'compile.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(work/'test'),str(work/'alarm.ppm')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=90);(work/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
assert before==hashes(),'sources changed during test'
from PIL import Image
out=root/'tools/verification';out.mkdir(exist_ok=True);Image.open(work/'alarm.ppm').save(out/'alarm-layout-0850.png')
result={'version':'0.8.50','passed':True,'result':json.loads(r.stdout.strip().splitlines()[-1]),'sourceHashes':{k:v for k,v in before.items() if k.startswith(sk.name+'/')},'testSourceHashes':{k:v for k,v in before.items() if not k.startswith(sk.name+'/')},'sanitizers':['ASan','UBSan'],'lvglArchiveSha256':hashlib.sha256((a.lvgl_build/'liblvgl.a').read_bytes()).hexdigest(),'limits':['Real LVGL9.6, Alarm, Focus and TextInput modules; NVS, FreeRTOS scheduling, RTC and audio output are host-controlled boundaries.','No device sound, RTC wake or timing measurement.','LSan unavailable; host LVGL uses CLIB allocator. Image uses test translations and Montserrat, not OS TTF.']}
(out/'alarm-theme-0850.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
