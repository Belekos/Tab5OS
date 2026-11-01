from pathlib import Path
import subprocess,json,hashlib,os
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture';production=release.parent/'libgit2-preflight-0826';library=release.parent/'libgit2-host-0822/git2.lib'
build=Path('C:/workspace/tab5-git-config-oom-0826');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    wrapper=build/'run.cmd';wrapper.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\npython "'+str(Path(__file__).resolve())+'"\nexit /b %errorlevel%\n')
    raise SystemExit(subprocess.call(['cmd','/c',str(wrapper)]))
sources=[production/'src/libgit2/config_list.c',production/'src/util/thread.c',here/'config_oom.cpp',here/'run_config_oom_0826.py']
before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources};libhash=hashlib.sha256(library.read_bytes()).hexdigest()
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/wd4996','/I'+str(sketch),str(here/'config_oom.cpp'),str(library),'winhttp.lib','rpcrt4.lib','crypt32.lib','ole32.lib','ws2_32.lib','secur32.lib','bcrypt.lib','advapi32.lib','/Fe:config_oom.exe'],cwd=build,check=True)
run=subprocess.run([str(build/'config_oom.exe')],capture_output=True,text=True);print(run.stdout);print(run.stderr);run.check_returncode()
assert before=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},'Production changed during test'
assert libhash==hashlib.sha256(library.read_bytes()).hexdigest(),'Library changed during test'
report=json.loads(run.stdout);report.update(version='0.8.26',physicalDeviceTested=False,scope='Real host libgit2 internal config lists, allocator fails all calls at/after each of 48 indices for new-key insertion, existing-key insertion and whole-list duplication, three rounds. Failed operations preserve source values and leave no copied output. Full library init/shutdown per case; zero tracked retained bytes after each case.',sources=before,hostLibrarySha256=libhash)
(here.parent/'verification/git-config-oom-0826.json').write_text(json.dumps(report,indent=2)+'\n')
