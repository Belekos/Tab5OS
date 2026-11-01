"""Production GitView + Explorer menus on real LVGL; task/Git/keystore/storage boundaries mocked; PIN modal is production."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--lvgl',required=True,type=Path);p.add_argument('--lvgl-build',required=True,type=Path);p.add_argument('--build',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.33_Ecriture';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True)
def block(text,signature):
 start=text.index(signature);i=text.index('{',start);depth=0;state='code'
 while i<len(text):
  c=text[i]
  if state=='line':
   if c in '\r\n':state='code'
  elif state=='comment':
   if text[i:i+2]=='*/':state='code';i+=1
  elif state in ('"',"'"):
   if c=='\\':i+=1
   elif c==state:state='code'
  elif text[i:i+2]=='//':state='line';i+=1
  elif text[i:i+2]=='/*':state='comment';i+=1
  elif c in ('"',"'"):state=c
  elif c=='{':depth+=1
  elif c=='}':
   depth-=1
   if not depth:return text[start:i+1]
  i+=1
 raise ValueError(signature)
inputs=['Tab5VaultPrompt.cpp','Tab5VaultPrompt.h','Tab5GitView.cpp','Tab5GitView.h','Tab5ExplorerMenus.inc','Tab5ExplorerGitContext.h','Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5UiStyle.cpp','Tab5UiStyle.h','Tab5ContextMenu.cpp','Tab5ContextMenu.h','WriterDocument.cpp','WriterDocument.h','Tab5KeyStore.h','Tab5Vault.h','Tab5VaultCore.h','WriterKeys.h','WriterUsb.h','WriterMouse.h','WriterGamepad.h','Tab5FileSelection.h','Tab5FileSort.h','lv_conf.h']
def hashes():
 paths=[*[sketch/n for n in inputs],*[p for p in here.rglob('*') if p.is_file() and '__pycache__' not in p.parts]]
 return {str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
original=hashes();mainPath=sketch/(sketch.name+'.ino');mainHash=hashlib.sha256(mainPath.read_bytes()).hexdigest();main=mainPath.read_text();parts={name:block(main,name) for name in ['static String fileGitDirectory() {','static bool selectedFolderIsGit() {']}
(work/'explorer_context.inc').write_text('\n'.join(parts.values()))
(work/'Tab5GitView.cpp').write_bytes((sketch/'Tab5GitView.cpp').read_bytes())
for path in (here/'stubs').rglob('*'):
 if path.is_file():dest=work/path.relative_to(here/'stubs');dest.parent.mkdir(exist_ok=True,parents=True);dest.write_bytes(path.read_bytes())
flags=['-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(work),'-I'+str(sketch),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include'),'-I'+str(a.lvgl.resolve()/'src')]
cmd=['g++',*flags,str(here/'ui.cpp'),*[str(sketch/x) for x in ['Tab5VaultPrompt.cpp','Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','Tab5ContextMenu.cpp','WriterDocument.cpp']],str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(work/'ui')]
r=subprocess.run(cmd,capture_output=True,text=True);(work/'compile.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
(work/'ui').chmod(0o755)
r=subprocess.run([str(work/'ui')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),timeout=90);print(r.stdout+r.stderr);(work/'run.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
assert original==hashes(),'UI sources changed during run'
assert mainHash==hashlib.sha256(mainPath.read_bytes()).hexdigest(),'Main integration changed during run'
report={'version':'0.8.33','passed':True,'physicalDeviceTested':False,'method':__doc__,'sanitizers':['address','undefined'],'sourceHashes':original,'integrationSourceHashes':{str(mainPath.relative_to(release)):mainHash},'extractedControllerHashes':{k:hashlib.sha256(v.encode()).hexdigest() for k,v in parts.items()},'tests':{str(p.relative_to(here)):hashlib.sha256(p.read_bytes()).hexdigest() for p in here.rglob('*') if p.is_file() and '__pycache__' not in p.parts},'result':json.loads(r.stdout)}
(release/'tools/verification/explorer-git-0833.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
