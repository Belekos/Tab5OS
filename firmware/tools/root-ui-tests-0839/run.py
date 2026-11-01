#!/usr/bin/env python3
"""Exact production orientation service and touch reader, USB modal/Focus with real LVGL, atomic Picture/Images writer, and native image-editor app."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,shutil
p=argparse.ArgumentParser();p.add_argument('--lvgl',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0836/host-dependencies/lvgl-9.6.0'));p.add_argument('--lvgl-build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_lvgl_0836'));p.add_argument('--build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0839/root-ui-tests'));p.add_argument('--draft',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;s=r/'Tab5OS_Arduino_v0.8.39_Ecriture';w=a.build.resolve();w.mkdir(parents=True,exist_ok=True)
shots=r/'tools/verification/root-ui-0839';shots.mkdir(parents=True,exist_ok=True)
def function(text,name):
 match=re.search(r'^static [^\n;]+\b'+name+r'\([^\n]*\)\s*\{',text,re.M);assert match,name
 start=match.start();p=text.index('{',match.start())+1;depth=1;quote=None;comment=None
 while depth:
  c=text[p];n=text[p:p+2]
  if comment=='line':
   if c=='\n':comment=None
  elif comment=='block':
   if n=='*/':comment=None;p+=1
  elif quote:
   if c=='\\':p+=1
   elif c==quote:quote=None
  elif n=='//':comment='line';p+=1
  elif n=='/*':comment='block';p+=1
  elif c in ['"',"'"]:quote=c
  elif c=='{':depth+=1
  elif c=='}':depth-=1
  p+=1
 return text[start:p]+'\n'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
paths=[s/n for n in ['Tab5OS_Arduino_v0.8.39_Ecriture.ino','Tab5ImageWrite.h','Tab5ImageEditor.cpp','Tab5DisplayOrientation.h','Tab5UsbFormatUi.cpp','Tab5UsbFormatUi.h','Tab5UsbStorage.h','Tab5UsbFormatPolicy.h','Tab5Tools.cpp','Tab5Focus.cpp','Tab5Focus.h','Tab5UiStyle.cpp','Tab5UiStyle.h','Tab5TextInput.cpp','Tab5TextInput.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','Tab5AppApi.h','lv_conf.h','sdcard/System/font.ttf','tools/native-apps/image-editor/app.c','tools/native-apps/image-editor/image_core.c','tools/native-apps/image-editor/image_core.h','tools/native-apps/image-editor/test_ui.c']]
original={str(p.relative_to(r)):sha(p) for p in paths};testpaths=[here/n for n in ['run.py','ui.cpp','image_write.cpp','native_picture.c']];testhash={str(p.relative_to(r)):sha(p) for p in testpaths}
main=(s/'Tab5OS_Arduino_v0.8.39_Ecriture.ino').read_text();(w/'orientation-extracted.inc').write_text('\n'.join(function(main,n) for n in ['touchRead','serviceDisplayOrientation']))
checks=[]
def static(ok,name):
 checks.append(dict(name=name,passed=bool(ok)));assert ok,name
static('bool screenUpsideDown = false;' in main,'fresh settings default to normal orientation')
static('append("screen_upside_down=%u\\n", settings.screenUpsideDown ? 1 : 0);' in main,'settings serializer persists orientation key')
static('key == "screen_upside_down") loadedSettings.screenUpsideDown = value.toInt() == 1;' in main,'settings parser restores explicit enabled value')
static('displayOrientationPending|=settings.screenUpsideDown!=loadedSettings.screenUpsideDown;' in main,'settings reload queues changed orientation')
static('settings.screenUpsideDown=lv_obj_has_state(lv_event_get_current_target_obj(e),LV_STATE_CHECKED);' in main,'settings switch updates runtime value')
rotations=re.findall(r'M5\.Display\.setRotation\((.*)\);',main)
static(len(rotations)>=7 and all('Tab5DisplayOrientation::rotation(' in line and 'settings.screenUpsideDown' in line for line in rotations),'all startup and reader rotation calls honor preference')
static(re.search(r'\bserviceDisplayOrientation\(\);',function(main,'touchRead')) is None,'touch callback does not change orientation during input')
static('serviceDisplayOrientation();' in main,'main loop services deferred orientation')
tools=(s/'Tab5Tools.cpp').read_text();key_start=tools.index('bool Tab5ToolsApp::key(');keybody=tools[key_start:key_start+1000]
static('Tab5UsbFormatUi::key' in keybody and keybody.index('Tab5UsbFormatUi::key')<keybody.index('if(!scope())'),'format modal handles keyboard before standalone Tools scope')
static('Tab5UsbFormatUi::service();' in tools,'Tools service updates modal progress')
image=(s/'Tab5ImageEditor.cpp').read_text();native=(s/'tools/native-apps/image-editor/app.c').read_text()
static('/Picture' in image and '/Images' not in image,'built-in image editor defaults exclusively to Picture')
static('"/Picture/' in native and '"/Images/' in native,'native image editor uses Picture with explicit legacy Images compatibility')
(shots/'source-integration.json').write_text(json.dumps(dict(checks=checks,passed=True,method='Source integration assertions; settings save/load are not executed on hardware.'),indent=2)+'\n')
base=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'];flags=['-std=c++17',*base,'-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(s),'-I'+str(w),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include')]
commands={
 'image-write':['g++',*flags,str(here/'image_write.cpp'),'-o',str(w/'image-write')],
 'ui':['g++',*flags,str(here/'ui.cpp'),*[str(s/n) for n in ['Tab5UsbFormatUi.cpp','Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','WriterDocument.cpp']],str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(w/'ui')],
 'native-picture':['gcc','-std=c11',*base,'-I'+str(s),'-I'+str(s/'tools/native-apps/image-editor'),str(here/'native_picture.c'),str(s/'tools/native-apps/image-editor/image_core.c'),'-o',str(w/'native-picture')],
 'native-image-ui':['gcc','-std=c11',*base,'-I'+str(s),str(s/'tools/native-apps/image-editor/test_ui.c'),str(s/'tools/native-apps/image-editor/image_core.c'),'-o',str(w/'native-image-ui')]
}
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
results={}
for name,cmd in commands.items():
 b=subprocess.run(cmd,capture_output=True,text=True);(w/(name+'-build.log')).write_text(b.stdout+b.stderr)
 if b.returncode:print(b.stdout+b.stderr);raise SystemExit(b.returncode)
 args=[str(w/name)]
 if name=='ui':args += [str(shots),str(s/'sdcard/System/font.ttf')]
 if name=='native-image-ui':
  nativeShots=shots/'native-image';nativeShots.mkdir(exist_ok=True);args += [str(nativeShots)]
 t=subprocess.run(args,capture_output=True,text=True,env=env,timeout=180);print(name+': '+t.stdout+t.stderr);(shots/(name+'-run.log')).write_text(t.stdout+t.stderr)
 if t.returncode:raise SystemExit(t.returncode)
 results[name]=json.loads(t.stdout.strip().splitlines()[-1])
from PIL import Image
for ppm in shots.rglob('*.ppm'):Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
final={str(p.relative_to(r)):sha(p) for p in paths};stable=original==final and testhash=={str(p.relative_to(r)):sha(p) for p in testpaths}
if not a.draft:assert stable,'source changed during tests; repeat after source freeze'
report=dict(version='0.8.39',passed=stable and not a.draft,draft=a.draft,physicalDeviceTested=False,sourceStableDuringTests=stable,method=__doc__,sourceHashes=original,testSourceHashes=testhash,sanitizers=['address','undefined'],result=results,checks=len(checks)+sum(v.get('checks',0) for v in results.values()),lvglArchiveSha256=sha(a.lvgl_build/'liblvgl.a'),evidenceHashes={str(p.relative_to(r)):sha(p) for p in sorted(shots.rglob('*')) if p.is_file()},limitations=['Orientation tests compile exact main touchRead and serviceDisplayOrientation source with real LVGL but a mock M5 display/touch; physical rotation, M5 touch calibration and tablet FPS are not measured. Settings persistence is checked by source integration assertions, not by executing the complete hardware settings store.','USB modal and focus/control styling are actual production LVGL code. USB attachment/requests/state/cancellation are deterministic mocks; this harness writes no removable media and does not prove firmware USB transport. USB formatter and driver use separate harnesses.','Atomic image writing uses production template code with an injected filesystem and failures. Native image editor is the actual app.c included by its shipped test_ui.c with the fake native API and tracked memory allocator.','The host LVGL archive is real LVGL9.6.0 compiled separately; new C/C++ test and production translation units are built with ASan/UBSan. Leak sanitizer is disabled for LVGL global allocations.'])
(r/'tools/verification/root-ui-0839.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n');print(json.dumps(dict(passed=report['passed'],checks=report['checks'],stable=stable,report=str(r/'tools/verification/root-ui-0839.json'))))
