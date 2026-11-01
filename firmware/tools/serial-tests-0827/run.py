from pathlib import Path
import subprocess,json,datetime,hashlib,re
release=Path(__file__).resolve().parents[2];sketch=release/'Tab5OS_Arduino_v0.8.27_Ecriture';tests=Path(__file__).parent
build=Path('C:/workspace/tab5-serial-tests-0827');build.mkdir(exist_ok=True)
source=(sketch/'Tab5Serial.cpp').read_text(encoding='utf-8');body=source[source.index('namespace Tab5Serial { namespace {'):]
(build/'serial-production.inc').write_text(body,encoding='utf-8')
vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
args=f'"{vc}" >nul && cl /nologo /std:c++17 /EHsc /utf-8 /O2 /D_CRT_SECURE_NO_WARNINGS /I"{sketch}" /I"{build}" "{tests/"backend.cpp"}" "{sketch/"Tab5SerialCommand.cpp"}" /Fe:"{build/"serial.exe"}" /Fo:"{build}/"'
compiled=subprocess.run('cmd.exe /d /s /c "'+args+'"',capture_output=True,text=True,encoding='utf-8',errors='replace');print(compiled.stdout+compiled.stderr)
result=subprocess.run([str(build/'serial.exe')],capture_output=True,text=True,encoding='utf-8',errors='replace') if compiled.returncode==0 else None
if result:print(result.stdout+result.stderr)
paths=[sketch/'Tab5Serial.cpp',sketch/'Tab5Serial.h',sketch/'Tab5SerialCore.h',sketch/'Tab5SerialCommand.cpp',tests/'backend.cpp',tests/'run.py']
report=dict(version='0.8.27',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),method='Exact production service namespace + fake UART/USB/NVS/FreeRTOS and real command parser, MSVC',physicalDeviceTested=False,passed=compiled.returncode==0 and result is not None and result.returncode==0,compileOutput=compiled.stdout+compiled.stderr,output=(result.stdout+result.stderr) if result else '',sourceHashes={str(p.relative_to(release)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
(release/'tools/verification/serial-backend-0827.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
