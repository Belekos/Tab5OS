"""Actual production Git wrapper and vendored libgit2; local POSIX interop.

Host adaptation changes only the generated vendor feature header: 64-bit host,
HTTPS disabled, SHA256 backed by OpenSSL. Source C bodies, SHA1dc, zlib, PCRE2,
Git wrapper, memory quota and journal/pull implementation remain production.
No mock repository, command dispatcher, index, commit or diff implementation.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,shutil,subprocess,tempfile
here=Path(__file__).resolve().parent;release=here.parent.parent
parser=argparse.ArgumentParser();parser.add_argument('--sketch',type=Path);parser.add_argument('--build',type=Path);args=parser.parse_args()
sketch=(args.sketch or release/'Tab5OS_Arduino_v0.8.32_Ecriture').resolve()
build=(args.build or Path(tempfile.mkdtemp(prefix='tab5-git-engine-0832-'))).resolve();build.mkdir(parents=True,exist_ok=True)
vendor=sketch/'src/tab5git';host=build/'vendor';shutil.copytree(vendor,host,dirs_exist_ok=True)
feature=host/'src/util/git2_features.h';text=feature.read_text()
replacements={'#define GIT_ARCH_32 1':'#define GIT_ARCH_64 1','#define GIT_HTTPS 1':'/* host local tests: no HTTPS */','#define GIT_MBEDTLS 1':'/* host local tests: no TLS */','#define GIT_SHA256_MBEDTLS 1':'#define GIT_SHA256_OPENSSL 1'}
for before,after in replacements.items():
 assert text.count(before)==1; text=text.replace(before,after)
feature.write_text(text)
inputs=[sketch/p for p in ['Tab5Git.cpp','Tab5Git.h','Tab5GitPort.cpp','Tab5GitPort.h','Tab5GitPull.cpp','Tab5GitPull.h','Tab5GitJournal.h','Tab5TerminalPath.h','Tab5FilePolicy.h']]+sorted(p for p in vendor.rglob('*') if p.is_file())+[here/p for p in ['run.py','cli.cpp','sha256_host.c','baseline_0831.cpp']]
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
snapshot={p.relative_to(release).as_posix():digest(p) for p in inputs}
flags=['-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-D_GNU_SOURCE','-I'+str(host/'include'),'-I'+str(host/'src/util'),'-I'+str(host/'src/libgit2')]
commands=[]
def compile_c(source):
 target=build/'obj'/source.relative_to(host).with_suffix('.o') if source.is_relative_to(host) else build/'sha256_host.o'
 target.parent.mkdir(parents=True,exist_ok=True)
 cmd=['gcc','-std=gnu17',*flags,'-Wno-deprecated-declarations','-c',str(source),'-o',str(target)]
 stamp=target.with_suffix('.sha256');key=hashlib.sha256((digest(source)+digest(feature)+json.dumps(cmd)).encode()).hexdigest()
 if not target.exists() or not stamp.exists() or stamp.read_text()!=key:
  result=subprocess.run(cmd,capture_output=True,text=True)
  if result.returncode:raise RuntimeError(str(source)+'\n'+result.stdout+result.stderr)
  stamp.write_text(key)
 return target
sources=sorted(host.rglob('*.c'))+[here/'sha256_host.c']
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:objects=list(pool.map(compile_c,sources))
client=build/'git_cli'
subprocess.run(['g++','-std=c++17',*flags,'-no-pie','-DTAB5_GIT_HOST','-I'+str(sketch),str(here/'cli.cpp'),str(sketch/'Tab5Git.cpp'),str(sketch/'Tab5GitPort.cpp'),str(sketch/'Tab5GitPull.cpp'),*[str(p) for p in objects],'-lcrypto','-o',str(client)],check=True)
print('Compiled production Git and',len(sources)-1,'vendor C units',flush=True)
volume=Path(tempfile.mkdtemp(prefix='volume-',dir=build))
checks=[]
def native(*words,cwd='/project',code=0,env=None):
 r=subprocess.run([str(client),str(volume),cwd,'git',*words],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',**(env or {})))
 checks.append({'args':list(words),'cwd':cwd,'code':r.returncode})
 assert r.returncode==code,(words,r.returncode,r.stdout,r.stderr)
 assert all(line.startswith('MEMORY 0 ') for line in r.stderr.splitlines() if line.startswith('MEMORY ')),r.stderr
 return r

def git(*words,cwd='project',code=0):
 r=subprocess.run(['git','-C',str(volume/cwd),*words],capture_output=True,text=True)
 assert r.returncode==code,(words,r.returncode,r.stdout,r.stderr)
 return r

native(cwd='/missing');native('--help',cwd='/missing');native('--version',cwd='/missing')
native('init','project',cwd='/');native('config','user.name','Tab5 Test');native('config','user.email','tab5@example.invalid')
project=volume/'project';(project/'sub').mkdir();(project/'other').mkdir()
(project/'a.txt').write_text('one\n');(project/'sub/b.txt').write_text('one\n');(project/'other/c.txt').write_text('one\n')
native('add','-A');native('commit','-m','Initial');git('fsck','--strict');assert git('status','--porcelain').stdout==''
# Subfolder . must never prepare modified siblings.
(project/'a.txt').write_text('two\n');(project/'sub/b.txt').write_text('two\n');(project/'other/c.txt').write_text('two\n')
r=native('status',cwd='/project/sub');assert 'PROJECT /project' in r.stderr
native('add','.',cwd='/project/sub');assert git('diff','--cached','--name-only').stdout=='sub/b.txt\n'
assert '+two' in native('diff','--cached','--','/project/sub/b.txt',cwd='/project/other').stdout
assert native('diff','--cached','--','/project/a.txt',cwd='/project/sub').stdout==''
# Literal selected file is independent of the folder currently displayed.
native('add','--','/project/a.txt',cwd='/project/sub');assert git('diff','--cached','--name-only').stdout=='a.txt\nsub/b.txt\n'
native('commit','-m','Selected edits',cwd='/project/sub');assert git('log','-1','--format=%s').stdout.strip()=='Selected edits'
assert ' M other/c.txt' in native('status').stdout
native('add','--','/project',cwd='/project/sub');native('commit','-m','Root directory');assert git('status','--porcelain').stdout==''
# Deletion of a selected directory removes exactly that directory from index.
(project/'sub/b.txt').unlink();(project/'sub').rmdir()
native('add','--','sub');assert git('diff','--cached','--name-status').stdout=='D\tsub/b.txt\n';native('commit','-m','Deleted directory')
(project/'empty').mkdir();native('add','--','empty');native('add','.',cwd='/project/empty')
native('add','--','absent',code=1);native('add','--','../outside',code=1)
native('diff','--','/outside',code=1);native('diff','--cached','--','/project/.git/config',code=1)
# A literal path with [] must not be interpreted as a pathspec glob.
(project/'literal[1].txt').write_text('before\n');(project/'literal1.txt').write_text('before\n');native('add','-A');native('commit','-m','Literal paths')
(project/'literal[1].txt').write_text('literal changed\n');(project/'literal1.txt').write_text('sibling changed\n')
r=native('diff','--','literal[1].txt');assert '+literal changed' in r.stdout and 'sibling changed' not in r.stdout
native('add','--','literal[1].txt');assert git('diff','--cached','--name-only').stdout=='literal[1].txt\n';native('commit','-m','Literal selection')
# Config genuinely persists; missing identity is a visible failure.
native('config','user.name','Changed author');assert git('config','user.name').stdout.strip()=='Changed author'
native('config','--unset','user.email');native('add','-A');assert 'user.name et user.email' in native('commit','-m','Denied',code=1).stderr
native('config','user.email','tab5@example.invalid');native('commit','-m','Restored identity');native('commit','-m','No changes',code=1)
# Packed objects produced by desktop Git remain usable in actual vendor code.
git('gc');assert 'Restored identity' in native('log','-n','5').stdout;git('fsck','--strict')
native('status',env={'TAB5_GIT_REPEAT':'200'})
# The unchanged .31 source must reproduce both context regressions on disk.
baseline=build/'git_cli_baseline_0831'
subprocess.run(['g++','-std=c++17',*flags,'-no-pie','-DTAB5_GIT_HOST','-I'+str(sketch),str(here/'cli.cpp'),str(here/'baseline_0831.cpp'),str(sketch/'Tab5GitPort.cpp'),str(sketch/'Tab5GitPull.cpp'),*[str(p) for p in objects],'-lcrypto','-o',str(baseline)],check=True)
(project/'other/c.txt').write_text('outside selected folder\n');(project/'empty/inside.txt').write_text('inside selected folder\n')
r=subprocess.run([str(baseline),str(volume),'/project/empty','git','add','.'],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1'))
assert r.returncode==0,(r.stdout,r.stderr)
wrong_scope=git('diff','--cached','--name-only').stdout.splitlines()
assert wrong_scope==['empty/inside.txt','other/c.txt'],wrong_scope
git('reset','--mixed','HEAD')
r=subprocess.run([str(baseline),str(volume),'/project/empty','git','add','--','/project'],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1'))
assert r.returncode!=0 and 'out_of_range' in r.stderr,(r.returncode,r.stdout,r.stderr)
baseline_result={'sourceSha256':digest(here/'baseline_0831.cpp'),'subdirectoryDotStagesSibling':wrong_scope,'rootPathExceptionExit':r.returncode,'rootPathException':'std::out_of_range'}
report={'version':'0.8.32','passed':True,'physicalDeviceTested':False,'networkTested':False,'method':__doc__.strip(),'checks':len(checks),'commands':checks,'repeatedStatusCycles':200,'vendorTranslationUnits':len(sources)-1,'sanitizers':['address','undefined'],'leakSanitizer':False,'hostFeatureChanges':replacements,'sourceHashes':snapshot,'clientSha256':digest(client),'featureSha256':digest(feature),'buildDirectory':str(build),'baselineRegression':baseline_result}
assert snapshot=={p.relative_to(release).as_posix():digest(p) for p in inputs},'Inputs changed during tests'
output=release/'tools/verification/git-engine-0832.json';output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'passed':True,'checks':len(checks),'report':str(output)}))
