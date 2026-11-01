from pathlib import Path
import subprocess,json,hashlib,os,shutil,uuid
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture';work=release.parent;build=Path('C:/workspace/tab5-git-0822')/uuid.uuid4().hex[:10];build.mkdir(parents=True)
shutil.copy2(here.parent/'network-tests/ShellPlatform.h',build/'ShellPlatform.h')
for f in ['unistd.h','dirent.h','fnmatch.h']:(build/f).write_text('#include "ShellPlatform.h"\n')
library=work/('libgit2-host-mbed-0822/git2.lib' if os.environ.get('TAB5_GIT_MBED_HOST') else 'libgit2-host-0822/git2.lib')
crypto=Path('C:/workspace/tab5-git-0822/mbedtls-3.6.5/mbedtls.lib')
cmd=['cl','/nologo','/std:c++17','/EHsc','/MD','/Z7','/utf-8','/wd4996','/wd4267','/DTAB5_GIT_HOST','/DNOMINMAX','/I'+str(build),'/I'+str(sketch),'/FI'+str(build/'ShellPlatform.h'),str(here/'git_cli.cpp'),str(sketch/'Tab5GitPull.cpp'),str(library),'winhttp.lib','rpcrt4.lib','crypt32.lib','ole32.lib','ws2_32.lib','secur32.lib','bcrypt.lib','advapi32.lib','dbghelp.lib','/Fe:git_cli.exe']
if os.environ.get('TAB5_GIT_MBED_HOST'):cmd.insert(-1,str(crypto))
cmd+=['/link','/DEBUG:FULL']
linked=[sketch/p for p in ['Tab5Git.cpp','Tab5Git.h','Tab5GitPort.cpp','Tab5GitPort.h','Tab5GitPull.cpp','Tab5GitPull.h','Tab5GitJournal.h','Tab5TerminalPath.h','Tab5FilePolicy.h']]+[here/'git_cli.cpp',here/'run_0825.py',library]+[work/'libgit2-preflight-0822'/p for p in ['src/libgit2/streams/mbedtls.c','src/libgit2/config_file.c','src/libgit2/config_list.c','src/util/thread.c']]
if os.environ.get('TAB5_GIT_MBED_HOST'):linked.append(crypto)
snapshot={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in linked}
subprocess.run(cmd,cwd=build,check=True)
assert snapshot=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in linked},'Linked inputs changed while compiling'
(Path('C:/workspace/tab5-git-0822')/'latest-client.txt').write_text(str(build/'git_cli.exe'))
metadata={'executable':str(build/'git_cli.exe'),'sha256':hashlib.sha256((build/'git_cli.exe').read_bytes()).hexdigest(),'sources':snapshot,'threads':False}
if os.environ.get('TAB5_GIT_MBED_HOST'):metadata['cryptoBackend']={'name':'mbedTLS','version':'3.6.5','sourceCommit':'ffb280bb63c78bfec1e1ab55040671768c85c923','library':str(crypto),'librarySha256':snapshot[str(crypto)]}
(build/'linked-sources.json').write_text(json.dumps(metadata,indent=2)+'\n')
vendor_inputs=[sketch/'src/tab5git'/p for p in ['src/libgit2/config_file.c','src/libgit2/config_list.c','src/util/thread.c','src/libgit2/streams/mbedtls.c']]
vendor_snapshot={p.relative_to(release).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in vendor_inputs}
if os.environ.get('TAB5_GIT_BUILD_ONLY'):raise SystemExit(0)
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/I'+str(sketch),str(here/'journal.cpp'),'/Fe:journal.exe'],cwd=build,check=True)
r=subprocess.run([str(build/'journal.exe')],capture_output=True,text=True);print(r.stdout);print(r.stderr);r.check_returncode();journal=json.loads(r.stdout)
journal['version']='0.8.25';journal['physicalDeviceTested']=False;journal['sources']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5GitJournal.h',sketch/'Tab5GitPort.cpp',here/'journal.cpp',here/'run_0825.py']};(here.parent/'verification/git-journal-0825.json').write_text(json.dumps(journal,indent=2)+'\n')
volume=build/('volume-'+uuid.uuid4().hex[:10]);volume.mkdir()
checks=[]
def native(*args,cwd='/project',code=0,env=None):
 r=subprocess.run([str(build/'git_cli.exe'),volume.as_posix(),cwd,'git',*args],capture_output=True,text=True,encoding='utf-8',env=env);checks.append({'args':args,'code':r.returncode});assert r.returncode==code,(args,r.returncode,r.stdout,r.stderr);assert 'TAB5_GIT_MEMORY 0 ' in r.stderr,r.stderr;return r
def git(*args,code=0):
 r=subprocess.run(['git','-C',str(volume/'project'),*args],capture_output=True,text=True,encoding='utf-8');assert r.returncode==code,(args,r.returncode,r.stdout,r.stderr);return r
native('init','project',cwd='/');native('config','user.name','Tab5 Test');native('config','user.email','tab5@example.invalid')
(volume/'project/a.txt').write_text('first\n',encoding='utf-8');(volume/'project/sub').mkdir(exist_ok=True);(volume/'project/sub/b.py').write_text('print("bonjour")\n')
native('add','-A');native('commit','-m','Initial');assert git('status','--porcelain').stdout=='',git('status','--porcelain').stdout;git('fsck','--strict')
(volume/'project/a.txt').write_text('second\n');assert ' M a.txt' in native('status').stdout;assert '+second' in native('diff').stdout
native('add','--','a.txt');assert '+second' in native('diff','--cached').stdout;native('commit','-m','Second');assert git('log','-1','--format=%s').stdout.strip()=='Second'
git('gc');assert 'Second' in native('log').stdout
(volume/'project/sub/b.py').write_text('print(42)\n');native('add','--','b.py',cwd='/project/sub');native('commit','-m','PC pack interop');git('fsck','--strict')
(volume/'project/sub/c.txt').write_text('directory staging\n');native('add','--','sub');assert 'sub/c.txt' in git('diff','--cached','--name-only').stdout;native('commit','-m','Directory selection')
(volume/'project/sub/c.txt').unlink();native('add','--','sub');assert 'D\tsub/c.txt' in git('diff','--cached','--name-status').stdout;native('commit','-m','Directory deletion')
native('init','/',cwd='/',code=1);native('init','usb:/',cwd='/',code=1)
native('remote','set-url','origin','https://example.invalid/repo.git');native('remote','set-url','origin','https://user:secret@example.invalid/repo.git',code=2)
git('remote','set-url','origin','https://user:secret@example.invalid/repo.git');r=native('remote','-v');assert 'secret' not in r.stdout and '***@' in r.stdout
r=native('fetch',code=1);assert 'secret' not in r.stdout+r.stderr
native('status',env=dict(os.environ,TAB5_GIT_REPEAT='200'))
for point in range(0,180,3):
 r=subprocess.run([str(build/'git_cli.exe'),volume.as_posix(),'/project','git','status'],capture_output=True,text=True,encoding='utf-8',env=dict(os.environ,TAB5_GIT_FAIL_ALLOC=str(point),TAB5_GIT_RECOVER_AFTER_FAILURE='1'))
 assert r.returncode in (0,1) and 'TAB5_GIT_MEMORY 0 ' in r.stderr,(point,r.returncode,r.stderr)
 checks.append({'injectedAllocationFailure':point,'code':r.returncode,'memory':r.stderr.strip().splitlines()[-3:]})
env=dict(os.environ,TAB5_GIT_CANCEL_AFTER='20');native('status',code=130,env=env);git('fsck','--strict')
pending=volume/'project/.git/tab5-pull.pending';pending.write_bytes(b'torn')
native('add','-A',code=1);native('config','user.name','Changed',code=1)
for args in [('status',),('log',),('diff',),('config','user.name'),('remote','-v')]:native(*args)
native('recover','--continue',code=1);assert pending.read_bytes()==b'torn';assert git('config','user.name').stdout.strip()=='Tab5 Test'
pending.unlink()
report={'version':'0.8.25','passed':True,'checks':len(checks),'commands':checks,'repeatedStatusCycles':200,'physicalDeviceTested':False,'scope':'Actual production Git command wrapper against official libgit2 host build; interoperates with installed PC Git including packed objects. After OOM, orphaned arena blocks are explicitly counted and reclaimed only after global shutdown. Each injected failure is followed by 3 healthy sessions in the SAME process; Windows handle count cannot increase. Separate FAT journal transition tests.','sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/n for n in ['Tab5Git.cpp','Tab5Git.h','Tab5GitJournal.h','Tab5GitPort.cpp','Tab5GitPort.h','Tab5GitPull.cpp','Tab5GitPull.h','Tab5TerminalPath.h','Tab5FilePolicy.h']]+[here/'git_cli.cpp',here/'run_0825.py']}}
assert snapshot=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in linked},'Linked inputs changed while testing'
assert vendor_snapshot=={p.relative_to(release).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in vendor_inputs},'Vendor inputs changed while testing'
report.update(vendorSources=vendor_snapshot,cryptoBackend=metadata.get('cryptoBackend'),threads=False,sourceStableDuringTest=True)
(here.parent/'verification/git-0825.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'checks':len(checks)}))
