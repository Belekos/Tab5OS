from pathlib import Path
import subprocess,json,hashlib
here=Path(__file__).resolve().parent;release=here.parent.parent
production=release.parent/'libgit2-preflight-0822/src/libgit2/streams/mbedtls.c'
build=Path('C:/workspace/tab5-git-tls-oom-0822');build.mkdir(exist_ok=True)
source=production.read_text(encoding='utf-8');digest=hashlib.sha256(production.read_bytes()).hexdigest()
start=source.index('#define GIT_SSL_DEFAULT_CIPHERS ');end=source.index('static int bio_read(',start)
(build/'tls-production.inc').write_text(source[start:end],encoding='utf-8');(build/'tls-ca.pem').write_text('Boundary fixture; parser is deliberately stubbed.\n')
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/wd4996','/I'+str(build),str(here/'tls_lifecycle.cpp'),'/Fe:tls_lifecycle.exe'],cwd=build,check=True)
run=subprocess.run([str(build/'tls_lifecycle.exe')],cwd=build,capture_output=True,text=True);print(run.stdout);print(run.stderr);run.check_returncode()
assert digest==hashlib.sha256(production.read_bytes()).hexdigest(),'Production changed during test'
report=json.loads(run.stdout);report.update(physicalDeviceTested=False,scope='Exact production global-init/shutdown function block extracted from libgit2 mbedtls.c. Allocation-owning mbedTLS API boundaries are stubbed to fail config, bundle, strdup, no ciphers, seed, shutdown-register; normal and all-known-ciphers success. This tests ownership paths, not real TLS/network interoperability.',sourcePath=str(production),sources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [production,here/'tls_lifecycle.cpp',here/'run_tls_lifecycle.py']})
(here.parent/'verification/git-tls-lifecycle-0822.json').write_text(json.dumps(report,indent=2)+'\n')
