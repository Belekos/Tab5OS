#!/usr/bin/env python3
"""Production native editor, core and real LVGL; only hardware/storage boundaries mocked."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,shutil,tempfile
p=argparse.ArgumentParser();p.add_argument('--lvgl',required=True,type=Path);p.add_argument('--lvgl-build',required=True,type=Path);p.add_argument('--build',required=True,type=Path);p.add_argument('--m5gfx',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.50_Ecriture';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True)
inputs=['Tab5ImageEditor.cpp','Tab5ImageEditor.h','Tab5ImageText.cpp','Tab5ImageText.h','Tab5LvglMemory.cpp','Tab5LvglMemory.h','Tab5ImageCore.c','tools/native-apps/image-editor/image_core.c','tools/native-apps/image-editor/image_core.h','tools/native-apps/image-editor/image_selection.h','Tab5ImageWrite.h','Tab5ImageCodec.cpp','Tab5ImageCodec.h','Tab5ImageDecode.cpp','Tab5ImageDecode.h','Tab5WebPPort.cpp','Tab5WebPPort.h','Tab5Cooperate.h','Tab5ImageFormats.h','src/jpeg/jpge.cpp','src/jpeg/jpge.h','Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5UiStyle.cpp','Tab5UiStyle.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','WriterUsb.h','WriterMouse.h','WriterGamepad.h','lv_conf.h','sdcard/System/font.ttf']
inputs += [str(p.relative_to(sketch)) for base in ['src/tab5webp','src/tab5png'] for p in (sketch/base).rglob('*') if p.is_file() and p.suffix in ('.c','.h')]
def hashes():
 paths=[*[sketch/n for n in inputs],*[p for p in here.rglob('*') if p.is_file() and '__pycache__' not in p.parts]]
 return {str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
original=hashes()
(work/'Tab5ImageEditor.cpp').write_bytes((sketch/'Tab5ImageEditor.cpp').read_bytes())
for path in (here/'stubs').rglob('*'):
 if path.is_file():dest=work/path.relative_to(here/'stubs');dest.parent.mkdir(exist_ok=True,parents=True);dest.write_bytes(path.read_bytes())
flags=['-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(work),'-I'+str(sketch),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include'),'-I'+str(a.lvgl.resolve()/'src'),'-I'+str(a.m5gfx.resolve()/'src')]
miniz_source=a.m5gfx.resolve()/'src/lgfx/utility/lgfx_miniz.c'
miniz_original=miniz_source.read_text();unaligned='\n#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1\n';assert miniz_original.count(unaligned)==1
miniz_host=work/'miniz_portable.c';miniz_host.write_text(miniz_original.replace(unaligned,'\n#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0\n'))
# RISC-V firmware uses this portable byte-access path; x86 upstream selects
# integer-pointer casts unsuitable for UBSan alignment checks. No suppression.
flags += ['-iquote',str(miniz_source.parent)]
jpeg_source=a.m5gfx.resolve()/'src/lgfx/utility/lgfx_tjpgd.c'
jpeg_host=work/'jpeg_portable.c';jpeg_text=jpeg_source.read_text()
for variable in ('v1','v4','v5','v6'):
 old=f'({variable} << 1)';assert jpeg_text.count(old)==1;jpeg_text=jpeg_text.replace(old,f'({variable} * 2)')
jpeg_host.write_text(jpeg_text)
# C defines multiplication for these bounded signed DCT coefficients; upstream
# doubles them with undefined negative left shifts. Only this host copy differs.
c_sources=[jpeg_host,miniz_host,sketch/'Tab5ImageCore.c',sketch/'src/tab5png/tab5_pngle.c',*sorted((sketch/'src/tab5webp').rglob('*.c'))]
objects=[];logs=[]
header_signature=hashlib.sha256(''.join(v for k,v in sorted(original.items()) if k.endswith('.h')).encode()).hexdigest()
for i,source in enumerate(c_sources):
 obj=work/('native-'+str(i)+'.o');objects.append(obj)
 signature=hashlib.sha256((hashlib.sha256(source.read_bytes()).hexdigest()+hashlib.sha256((sketch/'tools/native-apps/image-editor/image_core.c').read_bytes()+(sketch/'tools/native-apps/image-editor/image_selection.h').read_bytes()).hexdigest()+header_signature+repr(flags)).encode()).hexdigest();stamp=obj.with_suffix('.signature')
 if obj.exists() and stamp.exists() and stamp.read_text()==signature:continue
 cmd=['cc','-std=c11',*flags,'-c',str(source),'-o',str(obj)]
 r=subprocess.run(cmd,capture_output=True,text=True);logs.append(r.stdout+r.stderr)
 if r.returncode:(work/'compile.log').write_text(''.join(logs));print(r.stdout+r.stderr);raise SystemExit(r.returncode)
 stamp.write_text(signature)
native=work/'native-lvgl-adapter.o'
r=subprocess.run(['g++','-std=c++17',*flags,'-Dheap_caps_malloc=lvgl_host_malloc','-Dheap_caps_free=lvgl_host_free','-c',str(sketch/'Tab5LvglMemory.cpp'),'-o',str(native)],capture_output=True,text=True);logs.append(r.stdout+r.stderr)
if r.returncode: print(r.stdout+r.stderr);raise SystemExit(r.returncode)
objects.append(native)
cmd=['g++','-std=c++17',*flags,str(here/'ui.cpp'),*[str(sketch/x) for x in ['Tab5ImageText.cpp','Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','WriterDocument.cpp','Tab5ImageCodec.cpp','Tab5ImageDecode.cpp','Tab5WebPPort.cpp','src/jpeg/jpge.cpp']],*map(str,objects),str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(work/'ui')]
r=subprocess.run(cmd,capture_output=True,text=True);logs.append(r.stdout+r.stderr);(work/'compile.log').write_text(''.join(logs))
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
(work/'ui').chmod(0o755)
shots=release/'tools/verification/image-native-ui-0850';shutil.rmtree(shots,ignore_errors=True);shots.mkdir(parents=True,exist_ok=True)
sd=Path(tempfile.mkdtemp(prefix='sd-',dir=work));(sd/'Picture').mkdir()
from PIL import Image
wide=Image.new('RGB',(1300,300),(17,126,207));wide.save(sd/'Picture/full-size.jpg',quality=98);wide.save(sd/'Picture/full-size.bmp')
rgba=Image.new('RGBA',(173,127),(37,119,211,93));rgba.save(sd/'Picture/alpha.png');rgba.save(sd/'Picture/alpha.webp',lossless=True)
shutil.copyfile(sd/'Picture/alpha.png',sd/'Picture/renamed-image.dat');shutil.copyfile(sd/'Picture/full-size.jpg',sd/'Picture/misleading.t5i')

r=subprocess.run([str(work/'ui'),str(shots),str(sd),str(sketch/'sdcard/System/font.ttf')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=180);print(r.stdout+r.stderr);(work/'run.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
assert original==hashes(),'UI sources changed during run'
logs_dir=release/'tools/verification/drawing-ui-0850';logs_dir.mkdir(parents=True,exist_ok=True)
for name in ('compile.log','run.log'):shutil.copyfile(work/name,logs_dir/name)
from PIL import Image
for ppm in shots.glob('*.ppm'):
 Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
report={'version':'0.8.50','passed':True,'physicalDeviceTested':False,'method':__doc__,'sanitizers':['address','undefined'],'sourceHashes':{k:v for k,v in original.items() if k.startswith(sketch.name+'/')},'testSourceHashes':{k:v for k,v in original.items() if not k.startswith(sketch.name+'/')},'jpegHostPortablePath':{'sourceSha256':hashlib.sha256(jpeg_source.read_bytes()).hexdigest(),'compiledSha256':hashlib.sha256(jpeg_host.read_bytes()).hexdigest(),'reason':'Four upstream signed negative left shifts become multiplication by two; other decoder source is unchanged. Target SDK is unmodified.'},'minizHostPortablePath':{'sourceSha256':hashlib.sha256(miniz_source.read_bytes()).hexdigest(),'compiledSha256':hashlib.sha256(miniz_host.read_bytes()).hexdigest(),'reason':'Disable x86-only unaligned integer casts to execute the portable RISC-V firmware path under full UBSan.'},'lvglArchiveSha256':hashlib.sha256((a.lvgl_build/'liblvgl.a').read_bytes()).hexdigest(),'result':json.loads(r.stdout.strip().splitlines()[-1]),'evidenceHashes':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*shots.glob('*.png'),*logs_dir.glob('*.log'),*[p for p in [release/'tools/verification/drawing-core-0850.json'] if p.exists()]]},'limitations':['Filesystem is host disk with injected write failure; no tablet SD tested.','Memory admission, heap capability allocation, USB keyboard presence and file-picker result are mocked; native LVGL, focus, text-input, image-write transaction and image editor/core are production.','Host rendering cannot establish device FPS or PSRAM bandwidth.','Upstream M5GFX JPEG decoder is a host portable copy replacing four signed negative shifts by multiplication; target SDK is unmodified.']}
(release/'tools/verification/image-native-0850.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
