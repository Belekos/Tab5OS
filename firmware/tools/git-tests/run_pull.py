from pathlib import Path
import subprocess, json, hashlib, os, shutil, uuid
here=Path(__file__).resolve().parent
release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.22_Ecriture'
build=Path('C:/workspace/tab5-git-pull-0822')
build.mkdir(exist_ok=True)
shutil.copy2(here.parent/'network-tests/ShellPlatform.h',build/'ShellPlatform.h')
(build/'unistd.h').write_text('#include "ShellPlatform.h"\n')
library=release.parent/'libgit2-host-0822/git2.lib'
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/wd4996','/wd4267','/DTAB5_GIT_HOST','/DTAB5_GIT_PULL_TEST','/DNOMINMAX','/I'+str(build),'/I'+str(sketch),'/FI'+str(build/'ShellPlatform.h'),str(here/'pull.cpp'),str(sketch/'Tab5GitPull.cpp'),str(library),'winhttp.lib','rpcrt4.lib','crypt32.lib','ole32.lib','ws2_32.lib','secur32.lib','bcrypt.lib','advapi32.lib','/Fe:pull.exe'],cwd=build,check=True)
volume=build/('cases-'+uuid.uuid4().hex[:8]);volume.mkdir()
checks=[]
def git(repo,*args):
    p=subprocess.run(['git','-C',str(repo),*args],capture_output=True,text=True,encoding='utf-8');assert p.returncode==0,(args,p.stdout,p.stderr);return p.stdout.strip()
def setup(name):
    repo=volume/name;repo.mkdir();git(repo,'init','-b','main');git(repo,'config','user.name','Pull Test');git(repo,'config','user.email','test@example.invalid');git(repo,'config','core.autocrlf','false');git(repo,'config','core.filemode','false')
    for path,text in {'a.txt':'old a\n','b.txt':'old b\n','delete.txt':'delete me\n','keep.txt':'unchanged\n'}.items():(repo/path).write_text(text)
    git(repo,'add','.');git(repo,'commit','-m','old');old=git(repo,'rev-parse','HEAD')
    (repo/'a.txt').write_text('new a\n');(repo/'b.txt').write_text('new b\n');(repo/'delete.txt').unlink();(repo/'added.txt').write_text('newly added\n');(repo/'folder').mkdir();(repo/'folder/nested.txt').write_text('nested\n')
    git(repo,'add','-A');git(repo,'commit','-m','new');new=git(repo,'rev-parse','HEAD');git(repo,'reset','--hard',old);return repo,old,new
def native(repo,*args,code=0,crash=None,file=None,cancel=None,cancel_stage=None):
    env=dict(os.environ)
    if crash:env['TAB5_PULL_CRASH']=crash
    if file:env['TAB5_PULL_FILE']=str(file)
    if cancel:env['TAB5_PULL_CANCEL_AFTER']=str(cancel)
    if cancel_stage:env['TAB5_PULL_CANCEL_STAGE']=cancel_stage
    p=subprocess.run([str(build/'pull.exe'),str(repo),*args],capture_output=True,text=True,encoding='utf-8',env=env)
    assert p.returncode==code,(args,p.returncode,p.stdout,p.stderr)
    return p.stdout.strip()
def snapshot(repo):return {p.relative_to(repo).as_posix():p.read_bytes() for p in repo.rglob('*') if p.is_file() and '.git' not in p.relative_to(repo).parts}
def finish(repo,new):
    assert git(repo,'rev-parse','HEAD')==new
    assert git(repo,'status','--porcelain')==''
    assert native(repo,'pending')=='none';git(repo,'fsck','--strict')
repo,old,new=setup('normal');native(repo,'start',new);finish(repo,new);checks.append('normal fast-forward')
for stage,file in [('intent',None),('checkout-file',1),('checkout-file',2),('checkout-file',3),('checkout-file',4),('checkout-file',5),('checkout',None),('index',None),('head',None),('cleanup',None)]:
    repo,old,new=setup(stage+str(file or ''))
    native(repo,'start',new,code=97,crash=stage,file=file)
    assert git(repo,'rev-parse','HEAD')==(new if stage in ('head','cleanup') else old)
    if stage!='cleanup':
        assert native(repo,'pending')=='pending';before=snapshot(repo);native(repo,'start',new,code=1);assert snapshot(repo)==before
        native(repo,'resume')
    finish(repo,new);checks.append('process exit at '+stage+':'+str(file or ''))
for change in ['a.txt','added.txt','delete.txt','staged','branch','missing','corrupt']:
    repo,old,new=setup('protect-'+change.replace('.','-'))
    native(repo,'start',new,code=97,crash='checkout-file',file=2)
    if change=='staged':(repo/'keep.txt').write_text('staged manual\n');git(repo,'add','keep.txt')
    elif change=='branch':git(repo,'branch','other');git(repo,'symbolic-ref','HEAD','refs/heads/other')
    elif change=='missing':(repo/'a.txt').unlink()
    elif change=='corrupt':(repo/'.git/tab5-pull.pending').write_bytes(b'partial intent')
    else:(repo/change).write_text('manual divergent contents\n')
    before=snapshot(repo);index=(repo/'.git/index').read_bytes();head=git(repo,'rev-parse','HEAD')
    native(repo,'resume',code=1)
    assert before==snapshot(repo);assert index==(repo/'.git/index').read_bytes();assert head==git(repo,'rev-parse','HEAD');assert native(repo,'pending')=='pending'
    checks.append('preserve '+change)
for count in [1,2,10]:
    repo,old,new=setup('cancel-'+str(count));native(repo,'start',new,code=1,cancel=count)
    assert git(repo,'rev-parse','HEAD')==old
    if native(repo,'pending')=='pending':native(repo,'resume')
    else:native(repo,'start',new)
    finish(repo,new);checks.append('cancellation callback '+str(count))
for stage in ['checkout','index','head']:
    repo,old,new=setup('cancel-stage-'+stage);native(repo,'start',new,code=1,cancel_stage=stage)
    assert git(repo,'rev-parse','HEAD')==(new if stage=='head' else old)
    assert native(repo,'pending')=='pending';native(repo,'resume');finish(repo,new);checks.append('cancel at '+stage)
# Unrelated working edits and untracked files are not overwritten by recovery.
repo,old,new=setup('unrelated');native(repo,'start',new,code=97,crash='checkout-file',file=2)
(repo/'keep.txt').write_text('manual unchanged path\n');(repo/'personal.txt').write_text('personal\n');native(repo,'resume');assert (repo/'keep.txt').read_text()=='manual unchanged path\n';assert (repo/'personal.txt').read_text()=='personal\n';assert git(repo,'rev-parse','HEAD')==new;checks.append('preserve unrelated and untracked files')
report={'passed':True,'checks':len(checks),'cases':checks,'physicalDeviceTested':False,'scope':'Production Tab5GitPull.cpp with real libgit2 1.9.7, fresh process after forced _Exit during checkout and transaction transitions; PC Git independently validates HEAD/index/tree and fsck. Host filesystem, not FAT power-loss emulation.','sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5GitPull.h',sketch/'Tab5GitPull.cpp',here/'pull.cpp',here/'run_pull.py']}}
(here.parent/'verification/git-pull-0822.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'checks':len(checks)}))
