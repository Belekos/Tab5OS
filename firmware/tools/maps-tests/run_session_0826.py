from pathlib import Path
import hashlib,json,os,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-map-session-0826');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    command=build/'run.cmd'
    command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
def block(text,signature):
    a=text.index(signature);i=text.index('{',a);depth=1;end=i+1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[a:end]
engine=(sketch/'Tab5Map.cpp').read_text(encoding='utf-8')
text=(sketch/(sketch.name+'.ino')).read_text(encoding='utf-8')
names=['bool Tab5MapEngine::setRememberView(', 'static bool mapLoadStoredView(', 'static bool mapSaveStoredView(',
       'bool Tab5MapEngine::loadView(', 'bool Tab5MapEngine::saveView(', 'bool Tab5MapEngine::loadHomeView(', 'bool Tab5MapEngine::saveHomeView(']
ui=['static void restoreMapOpeningView()', 'static void saveMapLeavingView()', 'static void styleSettingsSwitch(',
    'static lv_obj_t* makeButton(', 'static lv_obj_t* createMapOverlay(', 'static void closeMapStylesEvent(',
    'static void mapRememberViewEvent(', 'static void mapStylesEvent(']
chunks=[block(engine,name) for name in names];uichunks=[block(text,name) for name in ui]
source=(here/'session.cpp').read_text(encoding='utf-8').replace('/* ENGINE */','\n'.join(chunks)).replace('/* UI */','\n'.join(uichunks))
(build/'session.cpp').write_text(source,encoding='utf-8')
assert 'mapViewDirty' not in text, 'Autosave flag/tick must be removed'
show=block(text,'static void showPage(Page page) {')
assert 'if (previousPage == Page::Map && page != Page::Map) {\n    saveMapLeavingView();' in show
assert 'if (previousPage != Page::Map) restoreMapOpeningView();' in show
assert '_rememberView=preferences.getBool("remember",true);' in engine
assert text.count('Tab5Map.saveView(')==1
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
flags=['/nologo','/utf-8','/fsanitize=address','/Zi','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch)]
result=subprocess.run(['cl',*flags,'/EHsc','/std:c++17',str(build/'session.cpp'),str(lvgl/'src/misc/lv_event.c'),str(lvgl/'src/core/lv_obj_event.c'),str(library),'/Fe:'+str(build/'session.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
(build/'compile.log').write_text(result.stdout+result.stderr,encoding='utf-8')
if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
result=subprocess.run([str(build/'session.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30)
print(result.stdout+result.stderr)
paths=[sketch/'Tab5Map.cpp',sketch/'Tab5Map.h',sketch/'Tab5MapViewState.h',sketch/'Tab5MapViewPolicy.h',here/'session.cpp',here/'run_session_0826.py']
report={'version':'0.8.26','passed':result.returncode==0,'physicalDeviceTested':False,
 'method':'Production storage methods and .ino open/close/switch/dialog callbacks extracted verbatim, compiled with MSVC ASan plus actual LVGL event dispatcher. NVS storage is mocked; production initializer NVS key and showPage hooks are statically asserted. 4000 unchanged view cycles, corrupt/partial/wrong-length/version/invalid-range records, legacy migration, NVS read/write failures, option disable/re-enable/reboot and 200 actual LVGL switch/Close cycles. No radio/network/SD or physical power-cut simulation.',
 'result':{'exitCode':result.returncode,'output':result.stdout+result.stderr},
 'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
 'controllers':{name:hashlib.sha256(value.encode()).hexdigest() for name,value in zip(names+ui,chunks+uichunks)}}
(release/'tools/verification/map-session-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(result.returncode)
