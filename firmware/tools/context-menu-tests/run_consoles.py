"""Exercise production console binding/refresh with real LVGL; workers simulated."""
from pathlib import Path
import datetime, hashlib, json, os, subprocess, sys
here=Path(__file__).resolve().parent
release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
build=Path('C:/workspace/tab5-consoles-0825');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    command=build/'run.cmd'
    command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
def block(text,key):
    start=text.index(key);brace=text.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
names=['Tab5Tools.cpp','Tab5Tools.h','Tab5ContextMenu.cpp','Tab5ContextMenu.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5Focus.cpp','Tab5Focus.h','Tab5UiStyle.cpp','Tab5UiStyle.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h']
hashes=lambda:{n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names}
before=hashes();production=(sketch/'Tab5Tools.cpp').read_text(encoding='utf-8');source=(here/'consoles.cpp').read_text(encoding='utf-8')
for token,key in {'BUTTON':'lv_obj_t* button(','BIND_CONSOLE':'void bindConsole(','VISIBLE':'static void visible(','CLEAR_ACTION':'if(action==212)','CLEAR_KEY':'if((e.ctrl||e.meta)&&!e.shift&&!e.alt&&e.key==K::Text','CLEAR_VISIBILITY':'if(a->consoleClear&&a->output)','TERMINAL_SERVICE':'if(a->tool==Tab5Tool::Terminal){','EXECUTABLE_SERVICE':'if(a->tool==Tab5Tool::Executable&&millis()','PYTHON_SERVICE':'if(a->tool==Tab5Tool::Python){'}.items():
    source=source.replace('/* '+token+' */',block(production,key))
(build/'consoles.cpp').write_text(source,encoding='utf-8')
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/DLV_FONT_MONTSERRAT_24=1','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch)]
result=subprocess.run(['cl',*flags,'/EHsc','/std:c++17',str(build/'consoles.cpp'),*[str(sketch/n) for n in ['Tab5TextInput.cpp','Tab5Focus.cpp','Tab5UiStyle.cpp','WriterDocument.cpp','Tab5ContextMenu.cpp']],str(lvgl/'src/font/lv_font_montserrat_24.c'),str(library),'/Fe:'+str(build/'consoles.exe')],cwd=build,capture_output=True,encoding='utf-8',errors='replace')
if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
result=subprocess.run([str(build/'consoles.exe')],cwd=build,capture_output=True,encoding='utf-8',errors='replace',timeout=60)
passed=result.returncode==0 and before==hashes()
report=dict(version='0.8.25',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=passed,exitCode=result.returncode,physicalDeviceTested=False,sourceStableDuringTests=before==hashes(),output=result.stdout+result.stderr,sources=before,tests={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),here/'consoles.cpp')},lvglLibrarySha256=hashlib.sha256(library.read_bytes()).hexdigest(),scope='Production Tools console binding, clear action, keyboard handler and three refresh branches extracted unchanged; real LVGL/TextInput/Focus/ContextMenu. Terminal, Python and ELF worker models simulated; no physical RAM or worker lifecycle claim.')
(release/'tools/verification/consoles-ui-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(result.stdout+result.stderr);raise SystemExit(0 if passed else 1)
