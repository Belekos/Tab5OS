#!/usr/bin/env python3
"""Production native editor, core and real LVGL; only hardware/storage boundaries mocked."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,shutil
p=argparse.ArgumentParser();p.add_argument('--lvgl',required=True,type=Path);p.add_argument('--lvgl-build',required=True,type=Path);p.add_argument('--build',required=True,type=Path);p.add_argument('--m5gfx',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.34_Ecriture';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True)
inputs=['Tab5ImageEditor.cpp','Tab5ImageEditor.h','Tab5ImageCore.c','tools/native-apps/image-editor/image_core.c','tools/native-apps/image-editor/image_core.h','Tab5ImageWrite.h','Tab5ImageCodec.cpp','Tab5ImageCodec.h','src/jpeg/jpge.cpp','src/jpeg/jpge.h','Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5UiStyle.cpp','Tab5UiStyle.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','WriterUsb.h','WriterMouse.h','WriterGamepad.h','lv_conf.h','sdcard/System/font.ttf']
def hashes():
 paths=[*[sketch/n for n in inputs],*[p for p in here.rglob('*') if p.is_file() and '__pycache__' not in p.parts]]
 return {str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
original=hashes()
(work/'Tab5ImageEditor.cpp').write_bytes((sketch/'Tab5ImageEditor.cpp').read_bytes())
for path in (here/'stubs').rglob('*'):
 if path.is_file():dest=work/path.relative_to(here/'stubs');dest.parent.mkdir(exist_ok=True,parents=True);dest.write_bytes(path.read_bytes())
flags=['-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(work),'-I'+str(sketch),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include'),'-I'+str(a.lvgl.resolve()/'src'),'-I'+str(a.m5gfx.resolve()/'src')]
commands=[['cc','-std=c11',*flags,'-c',str(a.m5gfx.resolve()/'src/lgfx/utility/lgfx_tjpgd.c'),'-o',str(work/'decoder.o')],['cc','-std=c11',*flags,'-c',str(sketch/'Tab5ImageCore.c'),'-o',str(work/'core.o')],['g++','-std=c++17',*flags,str(here/'ui.cpp'),*[str(sketch/x) for x in ['Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','WriterDocument.cpp','Tab5ImageCodec.cpp','src/jpeg/jpge.cpp']],str(work/'core.o'),str(work/'decoder.o'),str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(work/'ui')]]
for cmd in commands:
 r=subprocess.run(cmd,capture_output=True,text=True);(work/'compile.log').write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
(work/'ui').chmod(0o755)
shots=release/'tools/verification/image-native-ui-0834';shots.mkdir(parents=True,exist_ok=True)
sd=work/'sd';shutil.rmtree(sd,ignore_errors=True);sd.mkdir()
r=subprocess.run([str(work/'ui'),str(shots),str(sd),str(sketch/'sdcard/System/font.ttf')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=180);print(r.stdout+r.stderr);(work/'run.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
assert original==hashes(),'UI sources changed during run'
from PIL import Image
for ppm in shots.glob('*.ppm'):
 Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
report={'version':'0.8.34','passed':True,'physicalDeviceTested':False,'method':__doc__,'sanitizers':['address','undefined'],'sourceHashes':original,'decoderSourceSha256':hashlib.sha256((a.m5gfx.resolve()/'src/lgfx/utility/lgfx_tjpgd.c').read_bytes()).hexdigest(),'result':json.loads(r.stdout.strip().splitlines()[-1]),'evidenceHashes':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*shots.glob('*.png'),release/'tools/verification/image-core-0834.json',release/'tools/verification/image-render-0834.json']},'limitations':['Filesystem is host disk with injected write failure; no tablet SD tested.','Memory admission, heap capability allocation, USB keyboard presence and file-picker result are mocked; native LVGL, focus, text-input, image-write transaction and image editor/core are production.','Host rendering cannot establish device FPS or PSRAM bandwidth.']}
(release/'tools/verification/image-native-0834.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
