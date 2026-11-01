from pathlib import Path
import datetime, hashlib, json, re, subprocess
release=Path(__file__).resolve().parents[2]
sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture'
tests=Path(__file__).parent
build=Path('C:/workspace/tab5-battery-0828');build.mkdir(exist_ok=True)
vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
args=f'"{vc}" >nul && cl /nologo /std:c++17 /EHsc /utf-8 /Od /Zi /MT /fsanitize=address /DCONFIG_IDF_TARGET_ESP32P4 /I"{tests}" /I"{sketch}" "{tests/"battery.cpp"}" "{sketch/"Tab5BatteryGuard.cpp"}" /Fe:"{build/"battery.exe"}" /Fo:"{build}/" /Fd:"{build/"battery.pdb"}"'
compiled=subprocess.run('cmd.exe /d /s /c "'+args+'"',capture_output=True,text=True,encoding='utf-8',errors='replace')
print(compiled.stdout+compiled.stderr)
runs=[]
if compiled.returncode==0:
 for scenario in [None]+list(range(18)):
  command=[str(build/'battery.exe')]+([] if scenario is None else [str(scenario)])
  run=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30)
  output=run.stdout+run.stderr;print(scenario,output,end='')
  runs.append(dict(scenario='policy' if scenario is None else scenario,exitCode=run.returncode,output=output))
paths=[sketch/'Tab5BatteryGuard.h',sketch/'Tab5BatteryGuard.cpp',tests/'M5Unified.h',tests/'battery.cpp',tests/'run.py']
report=dict(version='0.8.28',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),method='Exact production policy and hardware adapter; fake I2C/board/power-off; MSVC AddressSanitizer',physicalDeviceTested=False,passed=compiled.returncode==0 and len(runs)==19 and all(r['exitCode']==0 for r in runs),checks=sum(int(m) for r in runs for m in re.findall(r'PASS (\d+) checks',r['output'])),compileOutput=compiled.stdout+compiled.stderr,runs=runs,sourceHashes={str(p.relative_to(release)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
(release/'tools/verification/battery-protection-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
