#!/usr/bin/env python3
"""Production Explorer layout/rows/helper functions and Focus with real LVGL and shipped TTF."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,shutil
p=argparse.ArgumentParser();p.add_argument('--lvgl',required=True,type=Path);p.add_argument('--lvgl-build',required=True,type=Path);p.add_argument('--build',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;s=r/'Tab5OS_Arduino_v0.8.38_Ecriture';w=a.build.resolve();w.mkdir(parents=True,exist_ok=True)
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
paths=[s/n for n in ['Tab5OS_Arduino_v0.8.38_Ecriture.ino','Tab5ExplorerUi.inc','Tab5ExplorerRowStyle.inc','Tab5Focus.cpp','Tab5Focus.h','Tab5UiStyle.cpp','Tab5UiStyle.h','Tab5TextInput.cpp','Tab5TextInput.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','Tab5Icons.h','lv_conf.h','sdcard/System/font.ttf']]+[here/'run.py',here/'ui.cpp']
original={str(p.relative_to(r)):sha(p) for p in paths}
text=(s/'Tab5OS_Arduino_v0.8.38_Ecriture.ino').read_text()
(w/'helpers-extracted.inc').write_text('\n'.join(function(text,n) for n in ['makeButton','makeIconButton','makeOpenWithButton','makeSearchButton','makeParentFolderButton','makeSortButton','createFileToolbar']))
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(s),'-I'+str(w),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include')]
cmd=['g++',*flags,str(here/'ui.cpp'),*[str(s/n) for n in ['Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','WriterDocument.cpp']],str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(w/'ui')]
b=subprocess.run(cmd,capture_output=True,text=True);(w/'build.log').write_text(b.stdout+b.stderr)
if b.returncode:print(b.stdout+b.stderr);raise SystemExit(b.returncode)
shots=r/'tools/verification/explorer-ui-0838';shutil.rmtree(shots,ignore_errors=True);shots.mkdir(parents=True)
t=subprocess.run([str(w/'ui'),str(shots),str(s/'sdcard/System/font.ttf')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=180);print(t.stdout+t.stderr);(shots/'run.log').write_text(t.stdout+t.stderr)
if t.returncode:raise SystemExit(t.returncode)
assert original=={str(p.relative_to(r)):sha(p) for p in paths},'source changed during tests'
from PIL import Image
for ppm in shots.glob('*.ppm'):Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
result=json.loads(t.stdout.strip().splitlines()[-1]);report=dict(version='0.8.38',passed=True,physicalDeviceTested=False,sourceStableDuringTests=True,method=__doc__,sourceHashes=original,sanitizers=['address','undefined'],result=result,checks=result['checks'],lvglArchiveSha256=sha(a.lvgl_build/'liblvgl.a'),evidenceHashes={str(p.relative_to(r)):sha(p) for p in sorted(shots.iterdir())},limitations=['createFilePage, Tab5ExplorerRowStyle, toolbar/button helpers, and native focus rendering use exact production source. updateFileActionState and storage/application callbacks are stubs; command availability, path labels, directory/search entries, and child-directory hints are fixtures. Input-to-content coherence is covered by the separate explorer-tests-0838 harness, not these screenshots.','Shell dynamic width registration is stubbed at the physical 1216x672 workspace.','Host tests do not measure tablet FPS or physical input transport.'])
(r/'tools/verification/explorer-ui-0838.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
