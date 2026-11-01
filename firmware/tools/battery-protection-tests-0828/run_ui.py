from pathlib import Path
import datetime,hashlib,json,subprocess,re
release=Path(__file__).resolve().parents[2];sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture';tests=Path(__file__).parent
build=Path('C:/workspace/tab5-battery-ui-0828');build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0826-host/lvgl.lib')
vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
args=f'"{vc}" >nul && cl /nologo /std:c++17 /EHsc /utf-8 /Od /Zi /MT /fsanitize=address /DLV_CONF_SKIP /DLV_USE_STDLIB_MALLOC=1 /DLV_LABEL_TEXT_SELECTION=1 /DLV_USE_OS=0 /DLV_FONT_MONTSERRAT_20=1 /I"{lvgl}" /I"{lvgl/"include"}" /I"{sketch}" "{tests/"ui.cpp"}" "{lvgl/"src/font/lv_font_montserrat_20.c"}" "{library}" /Fe:"{build/"battery-ui.exe"}" /Fo:"{build}/" /Fd:"{build/"battery-ui.pdb"}"'
compiled=subprocess.run('cmd.exe /d /s /c "'+args+'"',capture_output=True,text=True,encoding='utf-8',errors='replace');print(compiled.stdout+compiled.stderr)
result=subprocess.run([str(build/'battery-ui.exe')],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60) if compiled.returncode==0 else None
output=(result.stdout+result.stderr) if result else '';print(output)
paths=[sketch/'Tab5BatteryUi.inc',sketch/'Tab5BatteryGuard.h',tests/'ui.cpp',tests/'run_ui.py']
report=dict(version='0.8.28',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),method='Exact production BatteryUi include on real LVGL9.6; mocked sensor service, shutdown and cancellation/recovery APIs. UI compiled with MSVC AddressSanitizer; LVGL archive uninstrumented.',physicalDeviceTested=False,passed=compiled.returncode==0 and result is not None and result.returncode==0,checks=sum(map(int,re.findall(r'PASS (\d+) checks',output))),compileOutput=compiled.stdout+compiled.stderr,output=output,lvglArchiveSha256=hashlib.sha256(library.read_bytes()).hexdigest(),sourceHashes={str(p.relative_to(release)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
(release/'tools/verification/battery-ui-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
