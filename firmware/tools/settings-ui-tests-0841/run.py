#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,re,subprocess,sys,os
from extract import extract,remove_callbacks,closing
p=argparse.ArgumentParser();p.add_argument('--lvgl',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0841/host-dependencies/lvgl-9.6.0'));p.add_argument('--lvgl-build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_lvgl_0841'));p.add_argument('--build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0841/settings-ui'));a=p.parse_args()
t=Path(__file__).resolve().parent;r=t.parent.parent;s=r/'Tab5OS_Arduino_v0.8.41_Ecriture';w=a.build;w.mkdir(parents=True,exist_ok=True);o=r/'tools/verification/settings-ui-0841';o.mkdir(exist_ok=True)
for artifact in o.iterdir():
 if artifact.suffix in ['.png','.ppm'] and not artifact.name.startswith('.'):artifact.unlink()
main=s/(s.name+'.ino');source=main.read_text();names=['makeButton','makeSettingsCard','makeSettingsText','styleSettingsSlider','styleSettingsSwitch','styleSettingsDropdown','showSettingsSection','createSettingsPage']
functions={n:extract(source,n) for n in names}
gamepad=(s/'Tab5GamepadSettings.inc').read_text();functions['createGamepadSettings']=extract(gamepad,'createGamepadSettings')
serial=(s/'Tab5SerialSettings.inc').read_text();serial_globals=serial[:serial.index('static void serialSettingsRefresh')];functions['serialSettingsRefresh']=extract(serial,'serialSettingsRefresh');functions['createSerialSettings']=extract(serial,'createSerialSettings')
create=remove_callbacks(functions['createSettingsPage'].replace('lv_obj_add_event_cb(meshSettings,','TEST_KEEP_EVENT_CB(meshSettings,')).replace('TEST_KEEP_EVENT_CB(', 'lv_obj_add_event_cb(');create=re.sub(r'^  Tab5Mouse::[^\n]+\n','',create,flags=re.M)
order=names[:-1]+['createGamepadSettings','serialSettingsRefresh','createSerialSettings'];(w/'settings-production.inc').write_text(serial_globals+'\n'+'\n'.join(remove_callbacks(functions[n]) for n in order)+'\n'+create+'\n')
keys={json.loads('"'+k+'"') for k in re.findall(r'"((?:[^"\\]|\\.)*)"','\n'.join(functions.values()))};langs=[];header=['#include <map>\nstatic std::map<std::string,std::string> translations;\nstatic void language(unsigned which){translations.clear();switch(which){'];french=0
for index,file in enumerate(sorted((s/'sdcard/System/languages').glob('*.json'))):
 data=json.loads(file.read_text('utf-8-sig'));langs.append(file)
 if file.stem.startswith('fr'):french=index
 header.append('case '+str(index)+':')
 for key in sorted(keys):header.append('translations['+json.dumps(key,ensure_ascii=False)+']='+json.dumps(data.get('translations',{}).get(key,key),ensure_ascii=False)+';')
 header.append('break;')
header.append('}}\nstatic constexpr unsigned languageCount='+str(len(langs))+';static constexpr unsigned frenchIndex='+str(french)+';');(w/'settings-translations.inc').write_text('\n'.join(header))

popupNames=['updateSystemOverview','showSystemInfo','closeSystemInfo','makeButton','makeIconButton','makeSettingsCard','makeSettingsText','refreshDefaultFontButtons','chooseDefaultFont','resetDefaultFont']
popupFunctions={n:extract(source,n) for n in popupNames}
(w/'popup-helpers.inc').write_text('\n'.join(v for n,v in popupFunctions.items() if n not in ['updateSystemOverview','showSystemInfo','closeSystemInfo']))
(w/'system-overview.inc').write_text(popupFunctions['updateSystemOverview'])
(w/'system-info-integration.inc').write_text(popupFunctions['closeSystemInfo']+'\n'+popupFunctions['showSystemInfo'])

keyboard=[]
for marker in ['  if(Tab5FilePicker::visible()){Tab5FilePicker::key(e);return;}','  if(defaultFontPopup){if(e.key==K::Escape)','  if(systemInfoOverlay){']:
 start=source.index(marker);brace=source.index('{',start);keyboard.append(source[start:closing(source,brace)])
(w/'popup-keyboard.inc').write_text('static void modalKeyboard(const writing::KeyEvent& e){using K=writing::Key;\n'+'\n'.join(keyboard)+'\n}\n')

(w/'filepicker-production.inc').write_text(re.sub(r'^#include .*\n','',(s/'Tab5FilePicker.cpp').read_text(),flags=re.M))

popupLang=['#include <map>\nstatic std::map<std::string,std::string> popupTranslations;\nstatic void popupLanguage(unsigned lang){popupTranslations.clear();switch(lang){']
popupLanguageIds=[p.stem for p in langs]
for i,language in enumerate(popupLanguageIds):
 data=json.loads((s/'sdcard/System/languages'/(language+'.json')).read_text())
 popupLang.append('case '+str(i)+':')
 for key,value in data['translations'].items():
  if key in '\n'.join(popupFunctions.values())+(s/'Tab5DefaultFontUi.inc').read_text()+(s/'Tab5SystemInfoView.cpp').read_text():popupLang.append('popupTranslations['+json.dumps(key,ensure_ascii=False)+']='+json.dumps(value,ensure_ascii=False)+';')
 popupLang.append('break;')
popupLang.append('}}\nstatic const char* popupLanguageIds[]={'+','.join(json.dumps(p) for p in popupLanguageIds)+'};static constexpr unsigned popupLanguageCount='+str(len(popupLanguageIds))+';');(w/'popup-translations.inc').write_text('\n'.join(popupLang))


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
sourceFiles=[s/n for n in ['Tab5MemoryLayout.h','Tab5MemoryLayout.cpp','Tab5FirmwareFootprint.h','Tab5FirmwareFootprint.cpp','Tab5SettingsLayout.h','Tab5DefaultFontUi.inc','Tab5SystemInfoView.cpp','Tab5SystemInfoView.h','Tab5SerialSettings.inc','Tab5GamepadSettings.inc','Tab5UiStyle.cpp','Tab5Focus.cpp','Tab5FilePicker.cpp','sdcard/System/font.ttf']]+langs
hashes={str(p.relative_to(r)):sha(p) for p in sourceFiles};functionHashes={n:hashlib.sha256(v.encode()).hexdigest() for n,v in {**functions,**popupFunctions}.items()}
flags=['-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-cpp','-Wno-deprecated-declarations','-DLV_CONF_PATH="'+str(a.lvgl_build/'lv_conf.h')+'"','-I'+str(t/'mocks'),'-I'+str(w),'-I'+str(s),'-I'+str(a.lvgl)]

cmd=['g++',*flags,str(t/'popups.cpp'),*[str(s/n) for n in ['Tab5SystemInfoView.cpp','Tab5Focus.cpp','Tab5UiStyle.cpp','Tab5TextInput.cpp','WriterDocument.cpp']],str(a.lvgl_build/'liblvgl.a'),'-Wl,--wrap=lv_malloc,--wrap=lv_free','-pthread','-o',str(w/'popups')]
b=subprocess.run(cmd,capture_output=True,text=True);(o/'popup-build.log').write_text(b.stdout+b.stderr)
if b.returncode:print(b.stderr[-8000:]);raise SystemExit(b.returncode)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
popupRun=subprocess.run([str(w/'popups'),str(s/'sdcard/System/font.ttf'),str(o)],capture_output=True,text=True,env=env,timeout=180);(o/'popup-run.log').write_text(popupRun.stdout+popupRun.stderr);print(popupRun.stdout+popupRun.stderr);popupRun.check_returncode()

cmd=['g++',*flags,str(t/'layout.cpp'),str(a.lvgl_build/'liblvgl.a'),'-pthread','-o',str(w/'layout')]
b=subprocess.run(cmd,capture_output=True,text=True);(o/'layout-build.log').write_text(b.stdout+b.stderr)
if b.returncode:print(b.stderr[-8000:]);raise SystemExit(b.returncode)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
run=subprocess.run([str(w/'layout'),str(s/'sdcard/System/font.ttf'),str(o)],capture_output=True,text=True,env=env,timeout=180);(o/'layout-run.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr);run.check_returncode()
from PIL import Image
for p in o.glob('*.ppm'):Image.open(p).save(p.with_suffix('.png'));p.unlink()
integrationFiles=[main,s/'Tab5ShellUi.h',s/'Tab5ShellLayout.h',s/'Tab5GamepadSettings.inc',s/'Tab5SerialSettings.inc',s/'Tab5DefaultFontUi.inc']
report=dict(integrationSourceHashes={str(p.relative_to(r)):sha(p) for p in integrationFiles},version='0.8.41',passed=True,physicalDeviceTested=False,sourceHashes=hashes,mainFunctionHashes=functionHashes,testSourceHashes={str(p.relative_to(r)):sha(p) for p in t.rglob('*') if p.is_file() and '__pycache__' not in str(p)},evidenceHashes={str(p.relative_to(r)):sha(p) for p in o.iterdir() if p.is_file() and not p.name.startswith('.')},layout=json.loads(run.stdout.strip().splitlines()[-1]),popups=json.loads(popupRun.stdout.strip().splitlines()[-1]),languages=len(langs),lvglArchiveSha256=sha(a.lvgl_build/'liblvgl.a'),sourceStableDuringTests=hashes=={str(p.relative_to(r)):sha(p) for p in sourceFiles},method='Actual production Settings constructor/helpers/Gamepad and Serial builders extracted, with hardware action callbacks omitted. All coordinates, text, styles and Tab5SettingsLayout code remain actual production. Entire DefaultFontUi and SystemInfoView; actual main font callbacks and show/closeSystemInfo integration; full FilePicker source with only storage/DirectoryView boundary adapters. Actual Focus, UiStyle and TextInput linked. Real LVGL9.6 and shipped TTF at22/24, light/dark, every shipped translation,1216/1280 workspace and live resize.',limitations=['Hardware state/NVS/network/font-file admission are boundary doubles; tests do not exercise device configuration side effects.','Picker overlay, ownership, callbacks and close/focus are actual production; directory enumeration/content use an adapter with one TTF fixture.','Main/ShellUI integration hashes identify reviewed source, not a claim that the complete hardware main loop ran on host. Gamepad/Serial construction is executed; peripheral actions are omitted in the geometry harness.','Arabic/CJK geometry uses the shipped base font and its fallback; this harness does not configure the complete device font-selection/shaping pipeline.'])
report['confirmedFixes']=['System info renders the actual main overview formatter with disjoint HP/TCM/LP counters and valid/unknown cached firmware occupancy in all14languages.', 'Unchanged snapshots cause zero display refreshes, including shaped Arabic; transient snapshot storage is released on close/Escape, with safe OOM rendering fallback.','Settings System/Connections exposes Meshtastic and its actual callback passes the Settings scope.','Font picker and system popup preserve focus and safely tear down their modal children.']
report['sourceStableDuringTests'] &= all(hashlib.sha256(extract(main.read_text(),n).encode()).hexdigest()==functionHashes[n] for n in names+popupNames)
assert report['sourceStableDuringTests'];(o.parent/'settings-ui-0841.json').write_text(json.dumps(report,indent=2)+'\n')
