from pathlib import Path
import subprocess,datetime,hashlib,json
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-numeric-0826');build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');lib=Path('C:/workspace/tab5-ui-0822-host/lvgl.lib')
header=(sketch/'Tab5NumericKeyboard.h').read_text(encoding='utf-8');main=(sketch/(sketch.name+'.ino')).read_text(encoding='utf-8')
(build/'numeric_production.inc').write_text(header[header.index('namespace Tab5NumericKeyboard'):],encoding='utf-8')
start=main.index('static void keyPressedEvent(');end=main.index('\nstatic lv_obj_t* createKey(',start)
(build/'numeric_event.inc').write_text(main[start:end],encoding='utf-8')
ime=(sketch/'Tab5ImeUi.inc').read_text(encoding='utf-8')
assert 'const auto mode=Tab5NumericKeyboard::mode(input)?tab5ime::Mode::None:' in ime
assert 'Tab5NumericKeyboard::row(keyboardNumberMode,row)' in main
flags=['/nologo','/utf-8','/EHsc','/std:c++17','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(build)]
files=[sketch/'Tab5NumericKeyboard.h',sketch/'Tab5ImeUi.inc',sketch/(sketch.name+'.ino'),here/'numeric.cpp',Path(__file__)]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={p.relative_to(release).as_posix():sha(p) for p in files}
cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl '+' '.join('"'+arg+'"' for arg in flags+[str(here/'numeric.cpp'),str(lib),'/Fe:numeric.exe'])+'\nif errorlevel 1 exit /b 1\nnumeric.exe\n',encoding='utf-8')
r=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace');print(r.stdout+r.stderr)
stable=before=={p.relative_to(release).as_posix():sha(p) for p in files}
report=dict(version='0.8.26',passed=r.returncode==0 and stable,exitCode=r.returncode,physicalDeviceTested=False,sourceStableDuringTests=stable,checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),sources=before,output=r.stdout+r.stderr,lvglArchiveSha256=sha(lib),scope='Real LVGL, production numeric layout/field recognition and extracted touch event. App-specific dispatch and IME are boundaries; CJK numeric bypass and layout selection checked by source contract.')
(release/'tools/verification/numeric-keyboard-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
