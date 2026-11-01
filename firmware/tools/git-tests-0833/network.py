"""Real loopback smart HTTPS; production Git engine, OpenSSL host adaptation.
The isolated Git http-backend serves real repositories. No remote requests leave
loopback, no credentials or keys are retained in the release.
"""
from pathlib import Path
import argparse,base64,hashlib,json,os,subprocess,tempfile,shutil
from https_fixture import Fixture,certificates
p=argparse.ArgumentParser();p.add_argument('--client',type=Path,required=True);p.add_argument('--baseline',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.33_Ecriture'
case=Path(tempfile.mkdtemp(prefix='network-',dir=a.client.resolve().parent));certificates(case/'certs')
seed=case/'seed';remote=case/'remote.git';volume=case/'volume';volume.mkdir();checks=[]
ca=str(case/'certs/ca.pem');env=dict(os.environ,GIT_CONFIG_NOSYSTEM='1',GIT_CONFIG_GLOBAL=os.devnull,GIT_TERMINAL_PROMPT='0',GIT_SSL_CAINFO=ca,TAB5_GIT_TEST_CA=ca,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
auth='Authorization: Basic '+base64.b64encode(b'test:fixture-token').decode()
def pc(*args,cwd=seed,code=0):
 r=subprocess.run(['git',*args],cwd=cwd if cwd.exists() else None,env=env,text=True,capture_output=True,timeout=30)
 assert r.returncode==code,(args,r.stdout,r.stderr)
 return r.stdout.strip()
def native(*args,cwd='/project',code=0):
 a.client.chmod(0o755)
 r=subprocess.run([str(a.client.resolve()),str(volume),cwd,'git',*args],env=env,text=True,capture_output=True,timeout=30)
 print(args,r.returncode,r.stdout,r.stderr,flush=True)
 checks.append({'args':list(args),'cwd':cwd,'exit':r.returncode})
 assert r.returncode==code,(args,r.stdout,r.stderr)
 assert 'fixture-token' not in r.stdout+r.stderr
 assert all(s.startswith('MEMORY 0 ') for s in r.stderr.splitlines() if s.startswith('MEMORY '))
 return r
pc('init','--initial-branch=main',str(seed));pc('config','user.name','Server');pc('config','user.email','server@example.invalid')
(seed/'readme.txt').write_text('initial\n');pc('add','-A');pc('commit','-m','initial')
initial=pc('rev-parse','HEAD');pc('clone','--bare',str(seed),str(remote));pc('config','http.receivepack','true',cwd=remote)
(volume/'credentials').write_text('test\nfixture-token\n');fixture=Fixture(case,case/'certs')
pc('remote','add','origin',fixture.url);pc('config','http.extraHeader',auth)
def advance(text):
 (seed/'readme.txt').write_text(text+'\n');pc('add','-A');pc('commit','-m',text);pc('push','origin','main');return pc('rev-parse','HEAD')
def oid(repo,ref='HEAD'):return pc('rev-parse',ref,cwd=repo)
try:
 native('clone','--credential-file','/credentials',fixture.url,'/project',cwd='/')
 project=volume/'project';assert oid(project)==initial
 assert (project/'.git/shallow').exists(),'exercise default depth=1 clone'
 target=advance('remote two')
 fetched=native('fetch');assert 'appliquer les fichiers' in fetched.stdout or a.baseline;assert 'WORKTREE_CHANGED' not in fetched.stderr;assert oid(project)==initial and oid(project,'origin/main')==target
 pulled=native('pull','--ff-only');assert 'Fichiers et branche mis à jour' in pulled.stdout or a.baseline;assert ('WORKTREE_CHANGED' in pulled.stderr) or a.baseline;assert oid(project)==target and (project/'readme.txt').read_text()=='remote two\n'
 native('config','user.name','Tablet');native('config','user.email','tablet@example.invalid')
 (project/'tablet.txt').write_text('local\n');native('add','-A');native('commit','-m','tablet');local=oid(project);native('push');assert oid(remote)==local
 pc('fetch','origin');pc('reset','--hard','origin/main')
 # A renamed local branch retains tracking configuration from clone. The
 # update must target upstream main rather than invent remote/tablet-work.
 pc('branch','-m','tablet-work',cwd=project);target=advance('remote three')
 r=native('pull','--ff-only',code=1 if a.baseline else 0)
 if a.baseline:
  assert oid(project)==local
  (project/'tablet.txt').write_text('wrong branch reproduction\n');native('add','-A');native('commit','-m','wrong destination')
  native('push');assert oid(remote,'refs/heads/tablet-work')==oid(project) and oid(remote)==target
  baseline={'version':'0.8.33','passed':True,'baselineReproduced':True,'renamedBranchPullFails':True,'pushCreatesWrongRemoteBranch':True,'baselineSha256':hashlib.sha256((here/'baseline_0832.cpp').read_bytes()).hexdigest()};(release/'tools/verification/git-network-baseline-0833.json').write_text(json.dumps(baseline,indent=2)+'\n');print(json.dumps(baseline));raise SystemExit(0)
 assert oid(project)==target and (project/'readme.txt').read_text()=='remote three\n'
 (project/'tablet.txt').write_text('correct branch\n');native('add','-A');native('commit','-m','correct destination');native('push')
 assert oid(remote)==oid(project)
 assert pc('branch','--list','tablet-work',cwd=remote)==''
 # A local commit ahead of the last fetch is not a divergent history.
 r=native('pull','--ff-only');assert 'Dépôt déjà à jour' in r.stdout and 'WORKTREE_CHANGED' not in r.stderr
 (project/'ahead.txt').write_text('ahead\n');native('add','-A');native('commit','-m','ahead')
 r=native('pull','--ff-only');assert 'commits à envoyer' in r.stdout and 'WORKTREE_CHANGED' not in r.stderr
 native('push');assert oid(remote)==oid(project)
 # The default command follows the configured remote after remote rename.
 pc('remote','rename','origin','upstream',cwd=project)
 pc('fetch','origin');pc('reset','--hard','origin/main');target=advance('remote four')
 previous=oid(project);saved=project/'readme.txt';stamp=saved.stat().st_mtime_ns
 original=saved.read_bytes();saved.write_bytes(b'changed text!\n');os.utime(saved,ns=(stamp,stamp))
 r=native('pull','--ff-only',code=1);assert 'modifications locales' in r.stderr and 'WORKTREE_CHANGED' not in r.stderr
 assert oid(project)==previous and saved.read_bytes()==b'changed text!\n'
 assert not (project/'.git/tab5-pull.pending').exists()
 saved.write_bytes(original);r=native('pull','--ff-only');assert 'upstream/main' in r.stdout and oid(project)==target
 # Server-side rejection must never be reported as a completed push.
 (project/'ahead.txt').write_text('next\n');native('add','-A');native('commit','-m','rejected')
 hook=remote/'hooks/pre-receive';hook.write_text('#!/bin/sh\necho fixture-rejected >&2\nexit 1\n');hook.chmod(0o755)
 r=native('push',code=1);assert oid(remote)==target and 'rejet' in (r.stdout+r.stderr).lower();hook.unlink()
 native('push');assert oid(remote)==oid(project)
 # A stale remote tracking ref is not proof the remote branch still exists.
 before=oid(project);pc('update-ref','-d','refs/heads/main',cwd=remote)
 r=native('pull','--ff-only',code=1);assert 'n’existe plus' in r.stderr and oid(project)==before
 pc('update-ref','refs/heads/main',before,cwd=remote)
 # TLS certificate validation still runs before credential transmission.
 wrong=case/'wrong';certificates(wrong);trusted=env['TAB5_GIT_TEST_CA'];beforeRequests=len(fixture.requests)
 env['TAB5_GIT_TEST_CA']=str(wrong/'ca.pem');native('fetch',code=1);env['TAB5_GIT_TEST_CA']=trusted
 assert len(fixture.requests)==beforeRequests
 pc('fsck','--strict',cwd=project);pc('fsck','--strict',cwd=remote)
 report={'passed':True,'version':'0.8.33','physicalDeviceTested':False,'networkTested':'loopback smart HTTPS with OpenSSL host adaptation; ESP mbedTLS/network gate not simulated','checks':checks,'sourceHashes':{p.relative_to(release).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in [here/'network.py',here/'https_fixture.py',sketch/'Tab5Git.cpp',sketch/'Tab5GitPort.cpp',sketch/'Tab5GitPull.cpp']},'clientSha256':hashlib.sha256(a.client.read_bytes()).hexdigest()}
 (release/'tools/verification/git-https-0833.json').write_text(json.dumps(report,indent=2)+'\n')
finally:fixture.close()
