"""On-disk content freshness under coarse timestamps, plus contextual staging."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--client',type=Path,required=True);p.add_argument('--baseline-client',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.33_Ecriture';volume=Path(tempfile.mkdtemp(prefix='local-',dir=a.client.resolve().parent));project=volume/'project';checks=[]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1',GIT_CONFIG_NOSYSTEM='1',GIT_CONFIG_GLOBAL=os.devnull)
def git(*args):
 r=subprocess.run(['git','-C',str(project),*args],env=env,capture_output=True,text=True);assert r.returncode==0,(args,r.stdout,r.stderr);return r.stdout

def native(*args,baseline=False,code=0):
 binary=a.baseline_client if baseline else a.client;binary.chmod(0o755)
 r=subprocess.run([str(binary.resolve()),str(volume),'/project','git',*args],env=env,capture_output=True,text=True);assert r.returncode==code,(args,r.returncode,r.stdout,r.stderr)
 assert all(line.startswith('MEMORY 0 ') for line in r.stderr.splitlines() if line.startswith('MEMORY ')),r.stderr
 checks.append({'args':list(args),'baseline':baseline,'exit':r.returncode});return r.stdout
project.mkdir();git('init','--initial-branch=main');git('config','user.name','Test');git('config','user.email','test@example.invalid')
git('config','core.trustctime','false');git('config','core.checkstat','minimal')
(project/'sub').mkdir();(project/'other').mkdir()
for rel in ['sub/a.txt','other/b.txt']:
 f=project/rel;f.write_text('original\n');os.utime(f,(1600000000,1600000000))
git('add','-A');git('commit','-m','initial')
for rel in ['sub/a.txt','other/b.txt']:
 f=project/rel;f.write_text('modified\n');os.utime(f,(1600000000,1600000000))
# .32 takes the unchanged stat cache as proof of unchanged content. Confirm
# both its clean status and omitted staging before changing the implementation.
assert native('status',baseline=True)==''
assert native('diff','--','sub/a.txt',baseline=True)==''
native('add','-A',baseline=True)
assert git('diff','--cached','--name-only')==''
index=project/'.git/index';before=hashlib.sha256(index.read_bytes()).hexdigest()
status=native('status');assert ' M sub/a.txt' in status and ' M other/b.txt' in status
patch=native('diff','--','sub/a.txt');assert '+modified' in patch and 'other/b.txt' not in patch
assert hashlib.sha256(index.read_bytes()).hexdigest()==before,'read operations must not rewrite disk index'
native('add','--','sub');assert git('diff','--cached','--name-only')=='sub/a.txt\n'
native('commit','-m','only subtree');assert git('show','HEAD:sub/a.txt')=='modified\n' and git('show','HEAD:other/b.txt')=='original\n'
native('add','-A');native('commit','-m','rest');assert git('show','HEAD:other/b.txt')=='modified\n'
assert native('status')==''
# Restore exactly the saved timestamp again for another same-size edit.
f=project/'other/b.txt';stamp=f.stat().st_mtime_ns;f.write_text('repeated\n');os.utime(f,ns=(stamp,stamp))
assert ' M other/b.txt' in native('status');native('add','--','other/b.txt');native('commit','-m','repeat');assert git('show','HEAD:other/b.txt')=='repeated\n'
git('fsck','--strict')
report={'version':'0.8.33','passed':True,'physicalDeviceTested':False,'method':__doc__,'checks':checks,'baselineRegression':{'sameSizeSameTimestampStatusIncorrectlyClean':True,'sameSizeSameTimestampDiffEmpty':True,'sameSizeSameTimestampStageOmitted':True,'baselineSha256':hashlib.sha256((here/'baseline_0832.cpp').read_bytes()).hexdigest()},'sourceHashes':{p.relative_to(release).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in [here/'local.py',here/'baseline_0832.cpp',sketch/'Tab5Git.cpp']}}
(release/'tools/verification/git-local-0833.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'checks':len(checks)}))
