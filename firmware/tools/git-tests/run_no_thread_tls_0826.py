from pathlib import Path
import subprocess,json,hashlib,os
here=Path(__file__).resolve().parent;release=here.parent.parent
production=release.parent/'libgit2-preflight-0826/src/util/thread.c'
build=Path('C:/workspace/tab5-git-no-thread-tls-0826');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    wrapper=build/'run.cmd'
    wrapper.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\npython "'+str(Path(__file__).resolve())+'"\nexit /b %errorlevel%\n')
    raise SystemExit(subprocess.call(['cmd','/c',str(wrapper)]))
source=production.read_text(encoding='utf-8');digest=hashlib.sha256(production.read_bytes()).hexdigest()
start=source.index('#define TLSDATA_MAX');end=source.index('#elif defined(GIT_WIN32)',start)
(build/'thread-production.inc').write_text(source[start:end],encoding='utf-8')
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/FIinitializer_list','/I'+str(build),str(here/'no_thread_tls.cpp'),'/Fe:no_thread_tls.exe'],cwd=build,check=True)
run=subprocess.run([str(build/'no_thread_tls.exe')],capture_output=True,text=True);print(run.stdout);print(run.stderr);run.check_returncode()
assert digest==hashlib.sha256(production.read_bytes()).hexdigest(),'Production changed during test'
report=json.loads(run.stdout);report.update(version='0.8.26',physicalDeviceTested=False,scope='Exact production no-thread TLS implementation from libgit2 thread.c: 1000 full-table cycles, all 16 concurrent keys, allocation exhaustion, hole reuse, invalid/disposed bounds, null value, reentrant destructor, exactly-once destruction. Current P4 libgit2 has GIT_THREADS disabled.',sources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [production,here/'no_thread_tls.cpp',here/'run_no_thread_tls_0826.py']})
(here.parent/'verification/git-no-thread-tls-0826.json').write_text(json.dumps(report,indent=2)+'\n')
