from pathlib import Path
import subprocess,json,hashlib,os,datetime
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
build=Path('C:/workspace/tab5-elf-api5-0825');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    wrapper=build/'run.cmd';wrapper.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\npython "'+str(Path(__file__).resolve())+'"\nexit /b %errorlevel%\n')
    raise SystemExit(subprocess.call(['cmd','/c',str(wrapper)]))
paths=[sketch/'Tab5Executable.cpp',sketch/'Tab5AppApi.h',sketch/'Tab5AppPanel.h',here/'elf_api5.cpp',Path(__file__)]
hashes={str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
source=paths[0].read_text(encoding='utf-8')
def extract(signature):
    start=source.index(signature);opening=source.index('{',start);depth=1;at=opening+1
    while depth:
        depth+=(source[at]=='{')-(source[at]=='}');at+=1
    return source[start:at]
for name,signature in [('panel-closing','bool panelClosing()'),('stop','int appStop()'),('sleep','int appSleep('),('api4-prepare','const tab5_app_api4* prepareApi4('),('api4','const tab5_app_api4* appApi4()'),('api5','const tab5_app_api5* appApi5()')]:
    (build/f'elf-{name}-production.inc').write_text(extract(signature),encoding='utf-8')
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/I'+str(build),'/I'+str(sketch),str(here/'elf_api5.cpp'),'/Fe:elf_api5.exe'],cwd=build,check=True)
run=subprocess.run([str(build/'elf_api5.exe')],capture_output=True,text=True);print(run.stdout);print(run.stderr);run.check_returncode()
assert hashes=={str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'Production changed during test'
report=json.loads(run.stdout);report.update(version='0.8.25',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),physicalDeviceTested=False,scope='Exact production ABI4/5 accessors and cancellation callbacks extracted. Broker/cooperation/FreeRTOS boundaries modeled. Tests C ABI prefix, worker ownership, cancellation, borrowed synchronous network callback and absence of game controls for API5. Does not test physical network or UI.',sources=hashes)
(here.parent/'verification/elf-api5-0825.json').write_text(json.dumps(report,indent=2)+'\n')
