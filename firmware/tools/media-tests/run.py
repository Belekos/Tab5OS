from pathlib import Path
import os,sys,subprocess,shutil,json,hashlib,re
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-media-0826');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
for name in ['Tab5MediaList.cpp','Tab5MediaList.h','Tab5MediaSession.cpp','Tab5MediaSession.h','Tab5FileSort.h']:shutil.copy2(sketch/name,build/name)
for name in ['Preferences.h','session.cpp','list.cpp','catalogue.cpp','ui.cpp','Platform.h']:shutil.copy2(here/name,build/name)
for name in ['Arduino.h','Tab5Localization.h']:(build/name).write_text('#include "Platform.h"\n',encoding='utf-8')
audio=(sketch/'Tab5Audio.cpp').read_text('utf-8')
parts=re.findall(r'^static constexpr size_t MAX_RADIO_(?:CATALOGUE_BYTES|STATIONS) = [^\n]+',audio,re.M)
for name in ['addFallbackStations','loadStations']:
    begin=audio.index('void Tab5AudioEngine::'+name+'()');end=audio.index('\n}\n',begin)+2;parts.append(audio[begin:end])
(build/'catalogue-production.inc').write_text('\n'.join(parts)+'\n',encoding='utf-8')
main=(sketch/'Tab5OS_Arduino_v0.8.26_Ecriture.ino').read_text('utf-8')
def extract(name):
    match=re.search(r'static [^\n]+\b'+re.escape(name)+r'\([^\n]*\)\s*\{',main)
    if not match:raise RuntimeError('Missing media UI function '+name)
    i=match.end();depth=1
    while depth:
        if main[i]=='{':depth+=1
        elif main[i]=='}':depth-=1
        i+=1
    return main[match.start():i]
functions=['audioRememberPlayback','audioRestoreBrowser','audioRememberBrowser','audioRememberScroll','audioRememberItem','setAudioTabState','audioGroupBackEvent','audioFavoriteId','audioMediaMetadata','audioMediaFormat','audioMediaAction','audioChildGroup','refreshAudioList','audioLocalTabEvent','audioRadioTabEvent','audioRecordTabEvent','audioVideoTabEvent','audioReloadEvent','audioPlayEvent','audioVolumeEvent','createAudioPage']
globals_begin=main.index('static AudioView audioView =');globals_end=main.index('static uint32_t lastTopAudioUpdateMs = 0;',globals_begin)+len('static uint32_t lastTopAudioUpdateMs = 0;')
ui_functions={n:extract(n) for n in functions}
(build/'ui-production.inc').write_text(main[globals_begin:globals_end]+'\n'+'\n'.join(ui_functions.values())+'\n',encoding='utf-8')
flags=['/nologo','/utf-8','/EHsc','/std:c++17','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(build),'/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch),'/IC:/workspace/tab5-deps-0815/ArduinoJson/src','/DARDUINOJSON_ENABLE_ARDUINO_STRING=0']
reports=[]
for name,files in [('session',[build/'session.cpp',build/'Tab5MediaSession.cpp']),('list',[build/'list.cpp',sketch/'WriterDocument.cpp',sketch/'Tab5Focus.cpp',library]),('catalogue',[build/'catalogue.cpp']),('ui',[build/'ui.cpp',sketch/'WriterDocument.cpp',sketch/'Tab5Focus.cpp',lvgl/'src/font/lv_font_montserrat_24.c',library])]:
    extra=['/DLV_FONT_MONTSERRAT_24=1'] if name=='ui' else []
    c=subprocess.run(['cl',*flags,*extra,*map(str,files),'/Fe:'+str(build/(name+'.exe'))],cwd=build,capture_output=True);print(c.stdout.decode('utf-8','replace')+c.stderr.decode('utf-8','replace'),flush=True)
    if c.returncode:raise SystemExit(c.returncode)
    args=[str(build/(name+'.exe'))]
    if name=='catalogue':args.append(str(sketch/'sdcard/Packs/radios-internet.json'))
    r=subprocess.run(args,cwd=build,capture_output=True,timeout=60);output=r.stdout.decode('utf-8','replace')+r.stderr.decode('utf-8','replace');print(output,flush=True);reports.append(dict(name=name,exitCode=r.returncode,output=output))
sources=['Tab5MediaList.cpp','Tab5MediaList.h','Tab5MediaSession.cpp','Tab5MediaSession.h','Tab5Audio.cpp','Tab5Audio.h','Tab5FileSort.h','Tab5Focus.cpp','Tab5Focus.h','Tab5UiStyle.cpp','Tab5TextInput.cpp','WriterDocument.cpp','WriterKeys.h','Tab5Search.h','sdcard/Packs/radios-internet.json']
result=dict(version='0.8.26',physicalDeviceTested=False,passed=all(r['exitCode']==0 for r in reports),tests=reports,method='Production session with in-memory Preferences backend; production MediaList + actual LVGL/Focus; exact production radio loader with ArduinoJson and real600stationpack; extracted current main Media page and callbacks with simulated audio/SD. Audio::setVolume and hardware are covered by target compilation, not exercised here. No physical tablet tested.',sources={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in sources},mainGlobalsSha256=hashlib.sha256(main[globals_begin:globals_end].encode('utf-8')).hexdigest(),mainFunctionHashes={n:hashlib.sha256(body.encode('utf-8')).hexdigest() for n,body in ui_functions.items()},testSources={str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*here.glob('*.cpp'),*here.glob('*.h'),here/'run.py']})
dest=release/'tools/verification/media-0826.json';dest.parent.mkdir(exist_ok=True);dest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if result['passed'] else 1)
