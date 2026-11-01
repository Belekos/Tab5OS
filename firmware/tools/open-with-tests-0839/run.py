#!/usr/bin/env python3
"""Real MicroPython argv execution and extracted OpenWith functions on real LVGL."""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,subprocess,shutil
p=argparse.ArgumentParser();p.add_argument('--build',required=True,type=Path);p.add_argument('--lvgl',required=True,type=Path);p.add_argument('--lvgl-build',required=True,type=Path);p.add_argument('--draft',action='store_true');a=p.parse_args()
h=Path(__file__).resolve().parent;r=h.parent.parent;s=r/'Tab5OS_Arduino_v0.8.39_Ecriture';w=a.build.resolve();w.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def block(text,needle,body=False):
 assert text.count(needle)==1,(needle,text.count(needle))
 first=text.index(needle);start=text.index('{',first);i=start+1;depth=1;quote=None;comment=None
 while depth:
  c=text[i];two=text[i:i+2]
  if comment=='line':
   if c=='\n':comment=None
  elif comment=='block':
   if two=='*/':comment=None;i+=1
  elif quote:
   if c=='\\':i+=1
   elif c==quote:quote=None
  elif two=='//':comment='line';i+=1
  elif two=='/*':comment='block';i+=1
  elif c in ['"',"\x27"]:quote=c
  elif c=='{':depth+=1
  elif c=='}':depth-=1
  i+=1
 return text[start+1:i-1] if body else text[first:i]+'\n'
names=['Tab5OS_Arduino_v0.8.39_Ecriture.ino','Tab5Tools.cpp','Tab5Tools.h','Tab5Python.cpp','Tab5Python.h','Tab5OpenWith.h','Tab5PythonArguments.h','Tab5LaunchProfile.h','Tab5SystemPath.h','Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5UiStyle.cpp','Tab5UiStyle.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','mpconfigport.h','sdcard/System/font.ttf']
sources=sorted(set([s/n for n in names]+list((s/'src/micropython').rglob('*.c'))+list((s/'src/micropython').rglob('*.h'))+list((s/'py').rglob('*.h'))+list((s/'port').rglob('*.h'))+list((s/'genhdr').rglob('*.h'))))
before={str(x.relative_to(r)):sha(x) for x in sources};test_before={str(x.relative_to(r)):sha(x) for x in h.iterdir() if x.is_file()}
t=(s/'Tab5Tools.cpp').read_text()
integration=block((s/'Tab5OS_Arduino_v0.8.39_Ecriture.ino').read_text(),'toolsHooks.openNativeFile=')
for app,target in [('TextEditor','loadDocumentFrom'),('Reader','startReader'),('ImageViewer','startImageViewer'),('Drawing','openImageEditor'),('Audio','openAudioPath'),('Video','startVideoViewerSource')]:
 assert re.search(r'case Tab5NativeFileApp::'+app+r':[^;]*\b'+target+r'\(',integration),(app,target)
members=['lv_obj_t* label(','lv_obj_t* button(','lv_obj_t* dropdown(','void chooseOpenWith(','void cancelSwitch(','void confirmSwitch(','void askSwitch(','bool launchScript(','void printerSelection(','void refreshPrinters(']
(w/'members.inc').write_text('\n'.join(block(t,n) for n in members))
(w/'build.inc').write_text(block(t,'if(which==Tab5Tool::OpenWith){',True))
(w/'actions.inc').write_text(block(t,'if(tool==Tab5Tool::OpenWith){'))
(w/'public.inc').write_text('\n'.join(block(t,n) for n in ['void Tab5ToolsApp::openWith(','bool Tab5ToolsApp::resumePythonApp(','bool Tab5ToolsApp::openPythonApp(']))
(w/'switch.inc').write_text(block(t,'if(a->switchConfirmed&&!Tab5Python::running()){'))
(w/'printer-action.inc').write_text(block(t,'if(action==24){'))
printer_guard='if(a->tool==Tab5Tool::Print&&a->usbPrinterRevision!=Tab5UsbPrinter::revision())a->refreshPrinters();'
assert t.count(printer_guard)==1
(w/'printer-tick.inc').write_text(printer_guard+'\n')
log=[]
def run(command,**kwargs):
 result=subprocess.run(command,capture_output=True,text=True,**kwargs);log.append(result.stdout+result.stderr)
 if result.returncode:print(result.stdout+result.stderr);raise RuntimeError('Command failed: '+str(command[:2]))
 return result
# The production VM is compiled unchanged for the host ABI; no OS I/O is used
# by these argv programs. OS bridge functions are fixtures in vm.c.
vm=w/'vm-objects';vm.mkdir(exist_ok=True)
cflags=['gcc','-std=gnu11','-O1','-g','-I'+str(s),'-I'+str(s/'src/micropython')]
headers=hashlib.sha256(''.join(before[str(x.relative_to(r))] for x in sources if x.suffix=='.h').encode()).hexdigest()
def compile_c(source):
 obj=vm/(source.relative_to(s).as_posix().replace('/','_')+'.o');fingerprint=sha(source)+headers;cache=obj.with_suffix('.sha256')
 if not obj.exists() or not cache.exists() or cache.read_text()!=fingerprint:
  result=subprocess.run(cflags+['-c',str(source),'-o',str(obj)],capture_output=True,text=True)
  if result.returncode:raise RuntimeError(result.stdout+result.stderr)
  cache.write_text(fingerprint)
 return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile_c,sorted((s/'src/micropython').rglob('*.c'))))
run(cflags+[str(h/'vm.c'),*[str(x) for x in objects],'-lm','-o',str(w/'vm')]);vm_result=run([str(w/'vm')],timeout=90);vm_data=json.loads(vm_result.stdout.strip().splitlines()[-1])
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(s),'-I'+str(w),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include')]
run(['g++',*flags,str(h/'ui.cpp'),*[str(s/n) for n in ['Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','WriterDocument.cpp']],str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(w/'ui')])
e=r/'tools/verification/open-with-0839';e.mkdir(parents=True,exist_ok=True)
ui_result=run([str(w/'ui'),str(e),str(s/'sdcard/System/font.ttf')],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=120);ui_data=json.loads(ui_result.stdout.strip().splitlines()[-1])
from PIL import Image
for image in e.glob('*.ppm'):Image.open(image).save(image.with_suffix('.png'));image.unlink()
(e/'build-run.log').write_text('\n'.join(log))
after={str(x.relative_to(r)):sha(x) for x in sources};stable=before==after and test_before=={str(x.relative_to(r)):sha(x) for x in h.iterdir() if x.is_file()}
print(json.dumps(dict(vm=vm_data,ui=ui_data,stable=stable)))
if a.draft:raise SystemExit(0)
assert stable,'Source changed during tests; rerun after source freeze'
report=dict(version='0.8.39',passed=True,physicalDeviceTested=False,sourceStableDuringTests=True,method=__doc__,checks=vm_data['checks']+ui_data['checks'],result=dict(vm=vm_data,ui=ui_data,staticNativeRoutes=6),sourceHashes=before,testSourceHashes=test_before,evidenceHashes={str(x.relative_to(r)):sha(x) for x in sorted(e.iterdir()) if x.is_file()},sanitizers={'ui':['address','undefined'],'vm':[]},limitations=['OpenWith helper functions, chooser construction/action branches, Python switching functions and pending-switch service branch are extracted without changes. Storage/application execution and picker callbacks are simulated. The surrounding Tools shell is a fixture. The six root native routes are checked statically, not executed in this harness.','Actual complete production MicroPython VM executes the argv scenarios on the host ABI; OS bridge calls are fixtures and no ESP32 worker/hardware is exercised.','VM garbage collector reads conservative stack roots and is not instrumented with ASan; the routing/UI/path validation harness is instrumented with ASan/UBSan.'])
(r/'tools/verification/open-with-0839.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
