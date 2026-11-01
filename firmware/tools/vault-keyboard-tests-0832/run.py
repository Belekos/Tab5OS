"""Real LVGL PIN/dialog keyboard regressions; vault worker is a scripted double.

python3 tools/vault-keyboard-tests-0832/run.py --libraries /path/to/libraries
Build artifacts remain outside the release. No PIN crypto or hardware is claimed.
"""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, os, subprocess, sys, tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--libraries',required=True,type=Path)
p.add_argument('--build',type=Path)
p.add_argument('--jobs',type=int,default=4)
p.add_argument('--lvgl-archive',type=Path,help='Reuse a sanitized archive with matching sibling lv_conf.h')
a=p.parse_args();here=Path(__file__).resolve().parent;release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.32_Ecriture';lvgl=a.libraries.resolve()/'lvgl'
work=(a.build or Path(tempfile.mkdtemp(prefix='tab5-vault-keyboard-0832-'))).resolve();work.mkdir(parents=True,exist_ok=True)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def extract(text,signature):
 start=text.index(signature);i=text.index('{',start);depth=0;state='code'
 while i<len(text):
  c=text[i]
  if state=='line':
   if c in '\r\n':state='code'
  elif state=='block':
   if text[i:i+2]=='*/':state='code';i+=1
  elif state in ('"',"'"):
   if c=='\\':i+=1
   elif c==state:state='code'
  elif text[i:i+2]=='//':state='line';i+=1
  elif text[i:i+2]=='/*':state='block';i+=1
  elif c in ('"',"'"):state=c
  elif c=='{':depth+=1
  elif c=='}':
   depth-=1
   if not depth:return text[start:i+1]
  i+=1
 raise ValueError(signature)
assert 'version=9.6.0' in (lvgl/'library.properties').read_text()
config=(sketch/'lv_conf.h').read_text().replace('#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM','#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB')
(work/'lv_conf.h').write_text(config)
(work/'Arduino.h').write_text('#pragma once\n#include <cstdint>\n#include <string>\nusing String=std::string;\n')
flags=['-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(work/'lv_conf.h')+'"']
flags+=['-I'+str(x) for x in [work,lvgl,lvgl/'include',sketch]]
sys.path.insert(0,str(release/'tools/build_clang'));from prepare_lvgl import patch_shaper
patched,_,_=patch_shaper((lvgl/'src/misc/lv_text_ap.c').read_bytes());(work/'lv_text_ap.c').write_bytes(patched)
sources=[x for x in sorted((lvgl/'src').rglob('*')) if x.suffix in ('.c','.cpp','.S') and 'thorvg' not in str(x) and 'vg_lite_tvg' not in str(x)]
objects=work/'objects';objects.mkdir(exist_ok=True)
def compile_lv(source):
 obj=objects/('_'.join(source.relative_to(lvgl).parts)+'.o');stamp=obj.with_suffix('.sha256')
 actual=work/'lv_text_ap.c' if source.name=='lv_text_ap.c' else source
 signature=hashlib.sha256((sha(actual)+'\n'+repr(flags)).encode()).hexdigest()
 if obj.exists() and stamp.exists() and stamp.read_text()==signature:return obj
 result=subprocess.run(['g++' if source.suffix=='.cpp' else 'gcc',*flags,'-iquote',str(source.parent),'-c',str(actual),'-o',str(obj)],capture_output=True,text=True)
 if result.returncode:raise RuntimeError(result.stdout+result.stderr)
 stamp.write_text(signature);return obj
archive=work/'liblvgl.a'
if a.lvgl_archive:
 archive=a.lvgl_archive.resolve()
 assert archive.is_file() and (archive.parent/'lv_conf.h').read_text()==config,'Incompatible cached LVGL config'
else:
 with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,a.jobs)) as pool:compiled=list(pool.map(compile_lv,sources))
 subprocess.run(['ar','rcs',str(archive),*map(str,compiled)],check=True)
mainPath=sketch/(sketch.name+'.ino');mainHash=sha(mainPath);ino=mainPath.read_text();credential=(sketch/'Tab5CredentialUi.inc').read_text()
dialogs={x:extract(ino,x) for x in ['static bool keyboardObjectVisible(', 'static bool keyboardDialogScope(', 'static bool dispatchDialogKeyboard(']}
methods={x:extract(credential,x) for x in ['bool securityTool()', 'void securitySubmit(', 'bool securityKeyboard(', 'void updateCredentialActions(', 'bool validPin(']}
body=(here/'ui.cpp').read_text().replace('/* DIALOGS */','\n'.join(dialogs.values())).replace('/* CREDENTIAL_METHODS */','\n'.join(methods.values()))
(work/'ui.cpp').write_text(body)
production=['Tab5VaultPrompt.cpp','Tab5VaultPrompt.h','Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5UiStyle.cpp','Tab5UiStyle.h','WriterDocument.cpp','WriterDocument.h','Tab5CredentialUi.inc','Tab5Tools.cpp','Tab5KeyStore.h','Tab5Vault.h','Tab5VaultCore.h','WriterKeys.h','WriterUsb.h','WriterMouse.h','WriterGamepad.h','lv_conf.h']
boundSources=[*[sketch/x for x in production],release/'tools/build_clang/prepare_lvgl.py',*[p for p in here.rglob('*') if p.is_file() and '__pycache__' not in p.parts]]
sourceHashes={str(p.relative_to(release)):sha(p) for p in boundSources}
paths=[work/'ui.cpp',*[sketch/x for x in production if x.endswith('.cpp') and x!='Tab5Tools.cpp'],archive]
result=subprocess.run(['g++',*flags,*map(str,paths),'-lpthread','-o',str(work/'ui')],capture_output=True,text=True)
(work/'compile.log').write_text(result.stdout+result.stderr)
if result.returncode:raise RuntimeError(result.stdout+result.stderr)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
result=subprocess.run([str(work/'ui')],capture_output=True,text=True,env=env,timeout=120)
(work/'run.log').write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='')
if result.returncode:raise SystemExit(result.returncode)
stats=json.loads(result.stdout.strip().splitlines()[-1]);assert stats['passed']
assert sourceHashes=={str(p.relative_to(release)):sha(p) for p in boundSources},'UI sources changed during run'
assert mainHash==sha(mainPath),'Main integration changed during run'
report={'version':'0.8.32','test':'vault-keyboard','passed':True,'checks':stats['checks'],'cycles':stats['cycles'],
 'method':'Real LVGL 9.6, production VaultPrompt/Focus/TextInput/UiStyle, exact extracted dialog dispatcher and credential methods; scripted vault worker/status, UI action and OS hook doubles.',
 'limitations':['No physical keyboard, SD, crypto, worker scheduling or device PIN verification is performed.'],
 'sourceHashes':sourceHashes,
 'extractedSourceHashes':{x:hashlib.sha256(v.encode()).hexdigest() for x,v in {**dialogs,**methods}.items()},
 'integrationSourceHashes':{str(mainPath.relative_to(release)):mainHash},
 'lvglVersion':'9.6.0','lvglArchiveSha256':sha(archive),'sanitizers':['address','undefined'],'output':result.stdout}
out=release/'tools/verification/vault-keyboard-0832.json';out.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n');print(out)
