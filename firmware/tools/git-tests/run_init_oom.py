from pathlib import Path
import subprocess,json,hashlib
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.22_Ecriture';production=release.parent/'libgit2-preflight-0822';library=release.parent/'libgit2-host-0822/git2.lib'
build=Path('C:/workspace/tab5-git-init-oom-0822');build.mkdir(exist_ok=True)
sources=[production/'src/libgit2/sysdir.c',production/'src/libgit2/filter.c',production/'src/libgit2/merge_driver.c',production/'src/util/runtime.c',here/'init_oom.cpp',here/'run_init_oom.py']
before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/wd4996','/I'+str(sketch),str(here/'init_oom.cpp'),str(library),'winhttp.lib','rpcrt4.lib','crypt32.lib','ole32.lib','ws2_32.lib','secur32.lib','bcrypt.lib','advapi32.lib','/Fe:init_oom.exe'],cwd=build,check=True)
run=subprocess.run([str(build/'init_oom.exe')],capture_output=True,text=True);print(run.stdout);print(run.stderr);run.check_returncode()
assert before=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},'Production changed during test'
report=json.loads(run.stdout);report.update(physicalDeviceTested=False,scope='Real libgit2 host global initialization and shutdown with a custom allocator failing all allocation attempts at or after every index through normal-init count + 2, three sweeps. Every failed/successful session must release all tracked payload, followed by healthy reinitialization in the same process. Host WinHTTP; ESP mbedTLS initialization has a separate boundary test.',sources=before,hostLibrarySha256=hashlib.sha256(library.read_bytes()).hexdigest())
(here.parent/'verification/git-init-oom-0822.json').write_text(json.dumps(report,indent=2)+'\n')
