from pathlib import Path
import subprocess,json,datetime,hashlib
release=Path(__file__).resolve().parents[2];sketch=release/'Tab5OS_Arduino_v0.8.27_Ecriture';tests=Path(__file__).parent
build=Path('C:/workspace/tab5-serial-text-0827');build.mkdir(exist_ok=True)
vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
args=f'"{vc}" >nul && cl /nologo /std:c++17 /EHsc /utf-8 /O2 /fsanitize=address /I"{sketch}" "{tests/"text.cpp"}" "{sketch/"WriterDocument.cpp"}" /Fe:"{build/"text.exe"}" /Fo:"{build}/"'
compiled=subprocess.run('cmd.exe /d /s /c "'+args+'"',capture_output=True,text=True,encoding='utf-8',errors='replace');print(compiled.stdout+compiled.stderr)
result=subprocess.run([str(build/'text.exe')],capture_output=True,text=True,encoding='utf-8',errors='replace') if compiled.returncode==0 else None
if result:print(result.stdout+result.stderr)
paths=[sketch/'Tab5SerialText.h',sketch/'WriterDocument.cpp',tests/'text.cpp',tests/'run_text.py']
report=dict(version='0.8.27',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),method='Production UTF-8 decoder and independent WriterDocument UTF-8 validator, ASan',physicalDeviceTested=False,passed=compiled.returncode==0 and result is not None and result.returncode==0,compileOutput=compiled.stdout+compiled.stderr,output=(result.stdout+result.stderr) if result else '',sourceHashes={str(p.relative_to(release)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
(release/'tools/verification/serial-text-0827.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
