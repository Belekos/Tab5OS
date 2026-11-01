"""Production launch defaults with real ArduinoJson; NVS and storage boundaries modeled."""
from pathlib import Path
import datetime,hashlib,json,os,re,subprocess,sys
here=Path(__file__).resolve().parent
release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.23_Ecriture'
build=Path('C:/workspace/tab5-launch-defaults-0823')
build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    command=build/'run.cmd'
    command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
names=['Tab5LaunchDefaults.h','Tab5LaunchProfile.h','Tab5LaunchProfile.cpp','Tab5Json.h']
sources={name:digest(sketch/name) for name in names}
tests={name:digest(here/name) for name in ('run.py','defaults.cpp','esp_heap_caps.h')}
body=re.sub(r'^#include[^\n]*\n','',(sketch/'Tab5LaunchProfile.cpp').read_text(encoding='utf-8'),flags=re.M)
cpp=build/'defaults.cpp'
cpp.write_text((here/'defaults.cpp').read_text(encoding='utf-8').replace('/* PROFILE_IMPLEMENTATION */',body),encoding='utf-8')
arduinojson=Path('C:/workspace/tab5-deps-0815/ArduinoJson')
command=['cl','/nologo','/utf-8','/EHsc','/std:c++17','/D_CRT_SECURE_NO_WARNINGS','/I'+str(here),'/I'+str(sketch),'/I'+str(arduinojson/'src'),str(cpp),'/Fe:'+str(build/'defaults.exe')]
result=subprocess.run(command,cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
(build/'build.log').write_text(result.stdout+result.stderr,encoding='utf-8')
if result.returncode:
    print(result.stdout+result.stderr)
    raise SystemExit(result.returncode)
result=subprocess.run([str(build/'defaults.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8')
output=result.stdout+result.stderr
print(output,flush=True)
match=re.search(r'PASS (\d+) launch-defaults checks; groups=(\d+); cycles=(\d+); allocation_failures=(\d+); retained_bytes=(\d+); peak_bytes=(\d+); ArduinoJson=(\S+)',output)
stable=sources=={name:digest(sketch/name) for name in names}
report=dict(version='0.8.23',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=result.returncode==0 and bool(match) and stable,
            physicalDeviceTested=False,exitCode=result.returncode,output=output,sources=sources,tests=tests,sourceStableDuringTests=stable,
            checks=int(match[1]) if match else 0,groups=int(match[2]) if match else 0,cycles=int(match[3]) if match else 0,
            allocationFailures=int(match[4]) if match else 0,retainedBytes=int(match[5]) if match else None,peakBytes=int(match[6]) if match else 0,
            arduinoJsonVersion=match[7] if match else None,cases=re.findall(r'^CASE (.+)$',output,re.M),
            dependencySources={p.relative_to(arduinojson).as_posix():digest(p) for p in (arduinojson/'src').rglob('*') if p.is_file()},
            method='Real ArduinoJson with production Tab5Json PSRAM allocator and Tab5LaunchDefaults/Tab5LaunchProfile. Preferences and filesystem are modeled boundaries; actual save/load/forget functions execute.',
            limitations=['Tracked allocations are the JSON PSRAM adapter calls only, not all host heap.', 'No real NVS, SD/USB device or ELF task launch executed.'])
(release/'tools/verification/launch-defaults-0823.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
