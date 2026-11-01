"""Actual credential UI methods and secret input on LVGL; storage/crypto mocked."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,os
here=Path(__file__).resolve().parent;release=here.parent.parent;version=re.search(r'v(\d+\.\d+\.\d+)$',release.name).group(1);tag=version.replace('.','')
sketch=release/('Tab5OS_Arduino_v'+version+'_Ecriture');build=Path('C:/workspace/tab5-credentials-'+tag);build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0824-host/lvgl.lib')
def closing(text,start):
    depth=0;quote=None;escape=False;line=False;block=False;i=start
    while i<len(text):
        c=text[i];n=text[i:i+2]
        if line:
            if c=='\n':line=False
        elif block:
            if n=='*/':block=False;i+=1
        elif quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c==quote:quote=None
        elif n=='//':line=True;i+=1
        elif n=='/*':block=True;i+=1
        elif c in "\"'":quote=c
        elif c=='{':depth+=1
        elif c=='}':
            depth-=1
            if depth==0:return i+1
        i+=1
    raise ValueError('Unbalanced source')
def extract(text,name):
    m=re.search(r'^\s*(?:static )?(?:void|bool|unsigned|size_t|lv_obj_t\*)\s+'+name+r'\([^;]*?\)\s*\{',text,re.M)
    if not m:raise ValueError(name)
    return text[m.start():closing(text,text.index('{',m.start()))].strip()
source=(sketch/'Tab5Tools.cpp').read_text('utf-8')
names=['note','label','button','erasePassword','field','dropdown','refreshKeys','selectedKey','visible','enabled']
methods={n:extract(source,n) for n in names}
(build/'credential-helpers.inc').write_text('\n'.join(methods.values())+'\n',encoding='utf-8')
doc=(sketch/'WriterDocument.cpp').read_text('utf-8')
offsets={n:extract(doc,n) for n in ['byteOffset','charCount']}
(build/'utf8-production.inc').write_text('namespace writing{\n'+'\n'.join(offsets.values())+'\n}\n',encoding='utf-8')
ui=(sketch/'Tab5CredentialUi.inc').read_text('utf-8')
keys={json.loads('"'+k+'"') for k in re.findall(r'"((?:[^"\\]|\\.)*)"',ui)}
languages=[];translations=['static std::map<std::string,std::string> translations;\nstatic void language(unsigned which){translations.clear();switch(which){']
for index,path in enumerate(sorted((sketch/'sdcard/System/languages').glob('*.json'))):
    data=json.loads(path.read_text('utf-8-sig'));languages.append(path);translations.append('case '+str(index)+':')
    for key in sorted(keys):translations.append('translations['+json.dumps(key,ensure_ascii=False)+']='+json.dumps(data.get('translations',{}).get(key,key),ensure_ascii=False)+';')
    translations.append('break;')
translations.append('}}\nstatic constexpr unsigned languageCount='+str(len(languages))+';')
(build/'credential-translations.inc').write_text('\n'.join(translations),encoding='utf-8')
# Lifecycle adapter calls production resetSecurity, then deletes the LVGL root.
# Bind it to the current close/open ordering so omission cannot go unnoticed.
close=extract(source,'close');opening=extract(source,'open')
assert 'resetSecurity();auto* old=root;root=nullptr;' in close and close.index('resetSecurity();')<close.index('lv_obj_delete(old);')
assert opening.startswith('void open(') and 'close(true);if(root)return;' in opening
assert 'keyJob=securityBusy=false;' in (sketch/'Tab5CredentialUi.inc').read_text('utf-8')
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
flags=['/nologo','/utf-8','/EHsc','/std:c++17','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/DLV_FONT_MONTSERRAT_24=1','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch),'/I'+str(build)]
c=subprocess.run(['cl',*flags,str(here/'test_credentials.cpp'),str(sketch/'Tab5Focus.cpp'),str(lvgl/'src/font/lv_font_montserrat_24.c'),str(library),'/Fe:'+str(build/'credentials.exe')],cwd=build,capture_output=True)
print((c.stdout+c.stderr).decode('utf-8','replace')[-16000:],flush=True)
if c.returncode:raise SystemExit(c.returncode)
r=subprocess.run([str(build/'credentials.exe')],capture_output=True,timeout=90);output=(r.stdout+r.stderr).decode('utf-8','replace');print(output,flush=True)
files=[sketch/n for n in ['Tab5CredentialUi.inc','Tab5TextInput.cpp','Tab5TextInput.h','Tab5UiStyle.cpp','Tab5Focus.cpp','WriterKeys.h']]+[here/'test_credentials.cpp',Path(__file__),*languages]
report=dict(version=version,passed=r.returncode==0,exitCode=r.returncode,hardwareTested=False,output=output,method='Actual Tab5CredentialUi.inc and extracted Tools native helpers on LVGL9.6/font24. Backend KeyStore/Vault/Secrets/environment and shell open-close adapter mocked; current production close/open/reset ordering asserted and hashed. Actual TextInput with observed allocator boundary for wipe-before-free; actual UTF8 offset functions. This validates UI/state/privacy, not crypto or hardware.',sources={str(p.relative_to(release)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},toolsFunctionHashes={n:hashlib.sha256(v.encode('utf-8')).hexdigest() for n,v in dict(methods,close=close,open=opening).items()},utf8FunctionHashes={n:hashlib.sha256(v.encode('utf-8')).hexdigest() for n,v in offsets.items()},lvglArchiveSha256=hashlib.sha256(library.read_bytes()).hexdigest())
(release/('tools/verification/credential-ui-'+tag+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(r.returncode)
