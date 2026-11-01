#!/usr/bin/env python3
"""Actual Writing menu/button controllers and production mouse/focus on LVGL 9.6.
Document actions, storage and view rendering are boundaries, not exercised here.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,shutil
p=argparse.ArgumentParser();p.add_argument('--lvgl',type=Path,required=True);p.add_argument('--lvgl-build',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.52_Ecriture';build=a.build.resolve();build.mkdir(parents=True,exist_ok=True)
def block(text,signature):
 begin=text.index(signature);start=text.index('{',begin);depth=0;quote=None;escape=False;line=False;comment=False;i=start
 while i<len(text):
  c=text[i];n=text[i:i+2]
  if line:
   if c=='\n':line=False
  elif comment:
   if n=='*/':comment=False;i+=1
  elif quote:
   if escape:escape=False
   elif c=='\\':escape=True
   elif c==quote:quote=None
  elif n=='//':line=True;i+=1
  elif n=='/*':comment=True;i+=1
  elif c in "\"'":quote=c
  elif c=='{':depth+=1
  elif c=='}':
   depth-=1
   if not depth:return text[begin:i+1]
  i+=1
 raise ValueError(signature)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
production=['Tab5Mouse.cpp','Tab5Focus.cpp','Tab5UiStyle.cpp','Tab5TextInput.cpp','WriterDocument.cpp']
sources=sorted(set(['Tab5Writing.cpp','Tab5Writing.h',*production,*[n.replace('.cpp','.h') for n in production],'WriterKeys.h','WriterUsb.h','WriterMouse.h','WriterGamepad.h','WriterEncoding.h','sdcard/System/font.ttf']))
testPaths=[*[p for p in here.iterdir() if p.is_file()],*[p for p in (release/'tools/image-native-tests-0852/stubs').rglob('*') if p.is_file()]]
testHashes={str(p.relative_to(release)):sha(p) for p in testPaths}
sigs=['enum Action {','lv_obj_t* button(','void panel(','void closeMenu(','void toggleMenu(']
results=[]
for mode,base in [('baseline',a.baseline.resolve()),('current',sketch)]:
 before={n:sha(base/n) for n in sources};source=(base/'Tab5Writing.cpp').read_text();parts={sig:block(source,sig)+(';' if sig.startswith('enum ') else '') for sig in sigs}
 text=(here/'test.cpp').read_text()
 for sig,part in parts.items():text=text.replace('/* '+sig+' */',part)
 cpp=build/(mode+'.cpp');cpp.write_text(text)
 flags=['-Wno-deprecated-declarations','-Wno-cpp','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(a.lvgl.resolve()),'-I'+str(base),'-I'+str(release/'tools/image-native-tests-0852/stubs')]
 cmd=['g++',*flags,*(['-DBASELINE=1'] if mode=='baseline' else []),str(cpp),*[str(base/n) for n in production],str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(build/mode)]
 r=subprocess.run(cmd,text=True,capture_output=True);(build/('compile-'+mode+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
 shots=release/'tools/verification/writing-hover-ui-0852';shots.mkdir(parents=True,exist_ok=True)
 r=subprocess.run([str(build/mode),str(shots/mode),str(sketch/'sdcard/System/font.ttf')],text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=60);(build/('run-'+mode+'.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(r.returncode)
 assert before=={n:sha(base/n) for n in before},'Sources changed during tests'
 logs=release/'tools/verification/writing-hover-logs-0852';logs.mkdir(parents=True,exist_ok=True)
 for kind in ['compile','run']:shutil.copyfile(build/(kind+'-'+mode+'.log'),logs/(kind+'-'+mode+'.log'))
 results.append({'mode':mode,'sourceHashes':before,'controllers':{k:hashlib.sha256(v.encode()).hexdigest() for k,v in parts.items()},'result':json.loads(r.stdout.strip().splitlines()[-1])})
from PIL import Image
for ppm in shots.glob('*.ppm'):Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
assert testHashes=={str(p.relative_to(release)):sha(p) for p in testPaths},'Test sources changed during run'
report={'version':'0.8.52','passed':True,'physicalDeviceTested':False,'method':__doc__,'sanitizers':['address','undefined'],'lvglArchiveSha256':sha(a.lvgl_build/'liblvgl.a'),'results':results,'sourceHashes':{str(Path(sketch.name)/n):h for n,h in results[1]['sourceHashes'].items()},'testSourceHashes':testHashes,'evidenceHashes':{str(p.relative_to(release)):sha(p) for p in [*shots.glob('*.png'),*logs.glob('*.log')]},'limitations':['Only extracted production menu construction, button styling and menu closure execute from Tab5Writing.cpp; complete storage, editor rendering and command actions are not exercised.','USB reports are queued host fixtures; production Tab5Mouse service, Tab5Focus, Tab5UiStyle, Tab5TextInput and WriterDocument compile unchanged against real LVGL 9.6.','Host rendering uses the shipped OS font and does not establish performance, hardware USB behavior or display transfer correctness on Tab5.']}
(release/'tools/verification/writing-hover-0852.json').write_text(json.dumps(report,indent=2)+'\n')
