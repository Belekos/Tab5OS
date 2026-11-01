from pathlib import Path
import datetime, hashlib, json, re, subprocess
release=Path(__file__).resolve().parents[2]
sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture'
tests=Path(__file__).parent
build=Path('C:/workspace/tab5-compatibility-0828');build.mkdir(exist_ok=True)
paths=[sketch/n for n in ('Tab5Compatibility.cpp','Tab5Compatibility.h','Tab5CompatibilityPolicy.h','Tab5Audio.cpp','Tab5Audio.h')]
paths+=sorted(tests.rglob('*.h'))+[tests/'test.cpp',tests/'run.py']
before={str(p.relative_to(release)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
args=f'"{vc}" >nul && cl /nologo /std:c++17 /EHsc /utf-8 /Od /Zi /MT /fsanitize=address /I"{tests}" /I"{sketch}" "{tests/"test.cpp"}" "{sketch/"Tab5Compatibility.cpp"}" /Fe:"{build/"compatibility.exe"}" /Fo:"{build}/" /Fd:"{build/"compatibility.pdb"}"'
compiled=subprocess.run('cmd.exe /d /s /c "'+args+'"',capture_output=True,text=True,encoding='utf-8',errors='replace')
print(compiled.stdout+compiled.stderr)
output='';exit_code=None
if compiled.returncode==0:
    run=subprocess.run([str(build/'compatibility.exe')],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30)
    output=run.stdout+run.stderr;exit_code=run.returncode;print(output,end='')
after={str(p.relative_to(release)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
match=re.search(r'PASS (\d+) compatibility checks',output)
report=dict(version='0.8.28',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    method='Exact production compatibility policy and adapter; mocked I2C, chip information, ESP-Hosted RPC/events and critical-section SDK boundaries; MSVC AddressSanitizer; cached UI reads and concurrent transport events. Audio hooks are target-compiled separately, not exercised by this adapter harness.',
    physicalDeviceTested=False,passed=compiled.returncode==0 and exit_code==0 and bool(match) and before==after,
    sourceStableDuringTests=before==after,checks=int(match[1]) if match else 0,compileOutput=compiled.stdout+compiled.stderr,
    exitCode=exit_code,output=output,sourceHashes=before)
(release/'tools/verification/compatibility-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
