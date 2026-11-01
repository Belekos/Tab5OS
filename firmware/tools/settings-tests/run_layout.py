"""Execute current Settings construction and geometry on real LVGL, without hardware."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,os
here=Path(__file__).resolve().parent;release=here.parent.parent
version=re.search(r'v(\d+\.\d+\.\d+)$',release.name).group(1);tag=version.replace('.','')
sketch=release/('Tab5OS_Arduino_v'+version+'_Ecriture');main=sketch/(sketch.name+'.ino')
build=Path('C:/workspace/tab5-settings-layout-'+tag);build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0824-host/lvgl.lib')
def closing(text,start,left='{',right='}'):
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
        elif c==left:depth+=1
        elif c==right:
            depth-=1
            if depth==0:return i+1
        i+=1
    raise ValueError('unbalanced source')
def extract(text,name):
    m=re.search(r'static [^\n]+\b'+name+r'\([^;]*?\)\s*\{',text)
    if not m:raise ValueError(name)
    return text[m.start():closing(text,text.index('{',m.start()))]
def remove_callbacks(text):
    # Device mutation callbacks are outside this geometry test. The constructor,
    # labels, values, sizes, layout helpers and all style calls remain production.
    while True:
        m=re.search(r'lv_obj_add_event_cb\(',text)
        if not m:return text
        end=closing(text,m.end()-1,'(',')')
        text=text[:m.start()]+text[end:]
source=main.read_text('utf-8')
names=['makeButton','makeSettingsCard','makeSettingsText','styleSettingsSlider','styleSettingsSwitch','styleSettingsDropdown','createSettingsPage']
functions={name:extract(source,name) for name in names}
gamepad=(sketch/'Tab5GamepadSettings.inc').read_text('utf-8');functions['createGamepadSettings']=extract(gamepad,'createGamepadSettings')
serial=(sketch/'Tab5SerialSettings.inc').read_text('utf-8')
serial_globals=serial[:serial.index('static void serialSettingsRefresh')]
functions['serialSettingsRefresh']=extract(serial,'serialSettingsRefresh')
functions['createSerialSettings']=extract(serial,'createSerialSettings')
create=remove_callbacks(functions['createSettingsPage'])
create=re.sub(r'^  Tab5Mouse::[^\n]+\n','',create,flags=re.M)
order=names[:-1]+['createGamepadSettings','serialSettingsRefresh','createSerialSettings']
(build/'settings-production.inc').write_text(serial_globals+'\n'+'\n'.join(remove_callbacks(functions[n]) for n in order)+'\n'+create+'\n',encoding='utf-8')
# Real shipped translations for every string that can appear in the constructor.
keys=set(re.findall(r'"((?:[^"\\]|\\.)*)"','\n'.join(functions.values())))
keys={json.loads('"'+k+'"') for k in keys}
langs=[];header=['#include <map>\nstatic std::map<std::string,std::string> translations;\nstatic void language(unsigned which){translations.clear();switch(which){']
for index,file in enumerate(sorted((sketch/'sdcard/System/languages').glob('*.json'))):
    data=json.loads(file.read_text('utf-8-sig'));langs.append(file)
    header.append('case '+str(index)+':')
    for key in sorted(keys):
        value=data.get('translations',{}).get(key,key)
        header.append('translations['+json.dumps(key,ensure_ascii=False)+']='+json.dumps(value,ensure_ascii=False)+';')
    header.append('break;')
header.append('}}\nstatic constexpr unsigned languageCount='+str(len(langs))+';')
(build/'settings-translations.inc').write_text('\n'.join(header),encoding='utf-8')
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/DLV_USE_TINY_TTF=1','/DLV_TINY_TTF_FILE_SUPPORT=0','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch),'/I'+str(build),'/I'+str(release/'tools/runtime-tests')]
def run(args):
    r=subprocess.run(args,cwd=build,capture_output=True);out=(r.stdout+r.stderr).decode('utf-8','replace');print(out[-14000:],flush=True)
    if r.returncode:raise SystemExit(r.returncode)
run(['cl',*flags,'/std:c11','/c',str(lvgl/'src/font/tiny_ttf/lv_tiny_ttf.c')])
run(['cl',*flags,'/EHsc','/std:c++17',str(here/'test_layout.cpp'),str(library),str(build/'lv_tiny_ttf.obj'),'/Fe:'+str(build/'settings.exe')])
r=subprocess.run([str(build/'settings.exe'),str(sketch/'sdcard/System/font.ttf')],capture_output=True,timeout=120)
output=(r.stdout+r.stderr).decode('utf-8','replace');print(output,flush=True)
files=[sketch/'Tab5SettingsLayout.h',sketch/'Tab5UiStyle.cpp',sketch/'Tab5ShellLayout.h',sketch/'Tab5ShellPolicy.h',sketch/'sdcard/System/font.ttf',here/'test_layout.cpp',Path(__file__),*langs]
files.extend([sketch/'Tab5SerialCore.h',sketch/'Tab5SerialSettings.inc'])
report=dict(version=version,passed=r.returncode==0,exitCode=r.returncode,hardwareTested=False,languages=len(langs),output=output,method='Actual Settings constructor, card/text/switch/button helpers and gamepad card construction extracted verbatim, with device-action callbacks omitted; real LVGL 9.6, actual TinyTTF/font.ttf, shipped14translations, 1216/1280px workspace and live dock resize. Device services/NVS and input actions mocked. Geometry/style only, not device behavior.',mainFunctionHashes={n:hashlib.sha256(v.encode('utf-8')).hexdigest() for n,v in functions.items()},sources={str(f.relative_to(release)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},lvglArchiveSha256=hashlib.sha256(library.read_bytes()).hexdigest())
(release/('tools/verification/settings-layout-'+tag+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(r.returncode)
