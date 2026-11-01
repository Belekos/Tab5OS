from pathlib import Path
import subprocess,json,hashlib,os
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
production=sketch/'Tab5Executable.cpp';build=Path('C:/workspace/tab5-elf-lifecycle-0825');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    wrapper=build/'run.cmd';wrapper.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\npython "'+str(Path(__file__).resolve())+'"\nexit /b %errorlevel%\n')
    raise SystemExit(subprocess.call(['cmd','/c',str(wrapper)]))
source=production.read_text(encoding='utf-8');digest=hashlib.sha256(production.read_bytes()).hexdigest()
assert 'vTaskSetThreadLocalStoragePointerAndDelCallback' not in source
def function(signature,code=source):
    start=code.index(signature);opening=code.index('{',start);depth=1;at=opening+1
    while depth:
        depth+=(code[at]=='{')-(code[at]=='}');at+=1
    return code[start:at]
(build/'elf-task-production.inc').write_text(function('void task(void* argument)'),encoding='utf-8')
(build/'elf-service-production.inc').write_text(function('void tab5ExecutableService()'),encoding='utf-8')
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/I'+str(build),str(here/'elf_lifecycle.cpp'),'/Fe:elf_lifecycle.exe'],cwd=build,check=True)
run=subprocess.run([str(build/'elf_lifecycle.exe')],capture_output=True,text=True);print(run.stdout);print(run.stderr);run.check_returncode()
assert digest==hashlib.sha256(production.read_bytes()).hexdigest(),'Production changed during test'
baseline=release.parent/'Tab5OS_Ecriture_v0.8.22/Tab5OS_Arduino_v0.8.22_Ecriture/Tab5Executable.cpp'
baseline_source=baseline.read_text(encoding='utf-8')
(build/'elf-task-production.inc').write_text(function('void task(void* argument)',baseline_source),encoding='utf-8')
(build/'elf-service-production.inc').write_text(function('void tab5ExecutableService()',baseline_source),encoding='utf-8')
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/I'+str(build),str(here/'elf_lifecycle.cpp'),'/Fe:elf_lifecycle_baseline.exe'],cwd=build,check=True)
old_run=subprocess.run([str(build/'elf_lifecycle_baseline.exe')],capture_output=True,text=True)
assert old_run.returncode==2 and '!audioOwned' in old_run.stderr, old_run.stdout+old_run.stderr
(build/'elf-task-production.inc').write_text(function('void task(void* argument)'),encoding='utf-8')
(build/'elf-service-production.inc').write_text(function('void tab5ExecutableService()'),encoding='utf-8')
report=json.loads(run.stdout);report.update(version='0.8.25',physicalDeviceTested=False,baseline={'version':'0.8.22','sourceSha256':hashlib.sha256(baseline.read_bytes()).hexdigest(),'expectedFailure':True,'exitCode':old_run.returncode,'error':old_run.stderr.strip()},scope='Exact production ELF worker/service extracted. FreeRTOS/audio/ELF boundaries stubbed: ordinary worker context is freed synchronously at publication before worker returns; caps publication alone or blocked state cannot reclaim, suspended state must delete via SDK before cleanup. Audio release is forced to fail zero to three times before succeeding; the worker yields without publishing completion until ownership is released. Tests success/error/cancel/audio/argument variants. Does not emulate physical scheduler, SDK deletion, or execute ELF binaries.',sources={str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [production,sketch/'Tab5Task.h',here/'elf_lifecycle.cpp',here/'run_elf_lifecycle.py']})
(here.parent/'verification/elf-lifecycle-0825.json').write_text(json.dumps(report,indent=2)+'\n')
