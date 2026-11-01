"""Exercise the real command wrapper through loopback HTTPS and PC Git.

Private test keys, credentials and repositories are generated outside the release.
Run with --client <git_cli.exe>; --pc-only checks the server fixture separately.
"""
from pathlib import Path
import argparse,base64,hashlib,json,os,re,subprocess,time,uuid,shutil
from https_fixture import Fixture,certificates

here=Path(__file__).resolve().parent
release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--client',type=Path)
parser.add_argument('--pc-only',action='store_true')
args=parser.parse_args()
if not args.pc_only and not args.client:parser.error('--client is required unless --pc-only')
source_paths=[sketch/name for name in ['Tab5Git.cpp','Tab5Git.h','Tab5GitPort.cpp','Tab5GitPull.cpp']]
source_paths += [here/'https_fixture.py',here/'https_remote_0825.py',here/'git_cli.cpp',
                 sketch/'src/tab5git/src/libgit2/streams/mbedtls.c',sketch/'src/tab5git/UPSTREAM.json']
source_hashes={p.relative_to(release).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths if p.is_file()}
client_hash=hashlib.sha256(args.client.read_bytes()).hexdigest() if args.client else None
build_evidence={}
def verify_embedded_backend(source,released,upstream):
    """The Arduino vendor has an explicit prefix and relative include paths."""
    replacements={
        'src/libgit2/streams/mbedtls.c':[('streams/mbedtls.h','mbedtls.h'),('runtime.h','../../util/runtime.h'),
            ('stream.h','../stream.h'),('streams/socket.h','socket.h'),
            ('git2/transport.h','../../../include/git2/transport.h'),('util.h','../../util/util.h')],
        'src/libgit2/config_file.c':[('git2/config.h','../../include/git2/config.h'),
            ('git2/sys/config.h','../../include/git2/sys/config.h')]+
            [(name,'../util/'+name) for name in ['array.h','str.h','filebuf.h','regexp.h','wildmatch.h','hash.h']],
        'src/libgit2/config_list.c':[('hashmap_str.h','../util/hashmap_str.h')],
        'src/util/thread.c':[],
    }
    assert upstream in replacements,'Unexpected adapted native input'
    config=os.path.relpath(sketch/'src/tab5git/Tab5GitEmbeddedConfig.h',released.parent).replace('\\','/')
    prefix=f'/* Arduino translation unit; upstream license follows. */\n#include "{config}"\n'
    definitions=[('PCRE2_EXPORT',''),('PCRE2_EXP_DECL',''),('PCRE2_EXP_DEFN',''),('PCRE2_STATIC','1')]
    if upstream=='src/util/thread.c':
        definitions += [('SHA1DC_CUSTOM_INCLUDE_SHA1_C','"git2_util.h"'),
                        ('SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C','"git2_util.h"'),('SHA1DC_NO_STANDARD_INCLUDES','1')]
    definitions += [('_GNU_SOURCE','1'),('NDEBUG','1')]
    for name,value in definitions:
        prefix+=f'#ifndef {name}\n#define {name} {value}\n#endif\n'
    body=source.read_text(encoding='utf-8')
    for old,new in replacements[upstream]:
        body=body.replace(f'#include "{old}"',f'#include "{new}"')
    assert released.read_text(encoding='utf-8')==prefix+body,'Backend differs beyond documented Arduino adaptations'
if args.client:
    manifest_path=args.client.with_name('linked-sources.json')
    manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
    assert manifest['sha256']==client_hash,'The linked executable differs from its build manifest'
    linked_sources={};linked_vendor={}
    vendor=json.loads((release/'tools/verification/libgit2-vendor-0822.json').read_text(encoding='utf-8'))
    for name,digest in manifest['sources'].items():
        source=Path(name)
        assert hashlib.sha256(source.read_bytes()).hexdigest()==digest,('Compiled input changed',source.name)
        if source.is_relative_to(release):
            relative=source.relative_to(release).as_posix()
            source_hashes[relative]=digest;linked_sources[relative]=digest
        elif 'libgit2-preflight-0822' in source.parts:
            split=source.parts.index('libgit2-preflight-0822')
            upstream=Path(*source.parts[split+1:]).as_posix()
            released=sketch/'src/tab5git'/upstream
            released_hash=hashlib.sha256(released.read_bytes()).hexdigest()
            row=next(row for row in vendor['files'] if row['path']==upstream)
            assert row['patched_sha256']==digest and row['embedded_sha256']==released_hash,'Vendor manifest does not match tested inputs'
            verify_embedded_backend(source,released,upstream)
            relative=released.relative_to(release).as_posix()
            source_hashes[relative]=released_hash
            linked_sources['build/libgit2-preflight-0822/'+upstream]=digest
            linked_vendor[upstream]={'compiledSha256':digest,'embeddedSha256':released_hash,
                                    'adaptation':'Exact Arduino prefix and relative include substitutions verified'}
        else:
            linked_sources['build/'+source.parent.name+'/'+source.name]=digest
    build_evidence={'manifestSha256':hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                    'linkedSources':linked_sources,'linkedVendorInputs':linked_vendor}
    if 'threads' in manifest:
        build_evidence['threads']=manifest['threads']
    if 'cryptoBackend' in manifest:
        build_evidence['cryptoBackend']=manifest['cryptoBackend']
scratch=Path('C:/workspace/tab5-git-0822')
case=scratch/('https-'+uuid.uuid4().hex[:10]);case.mkdir(parents=True)
certificates(scratch)
wrong=case/'wrong-ca';certificates(wrong)
seed=case/'seed';remote=case/'remote.git';volume=case/'volume';volume.mkdir()
checks=[]
base_env=dict(os.environ,GIT_CONFIG_NOSYSTEM='1',GIT_CONFIG_GLOBAL=os.devnull,
              GIT_TERMINAL_PROMPT='0',GIT_SSL_CAINFO=str(scratch/'ca.pem'))
def pc(*command,cwd=None,expected=0):
    result=subprocess.run(['git',*command],cwd=cwd,env=base_env,
                          capture_output=True,text=True,encoding='utf-8',timeout=30)
    if expected is not None:assert result.returncode==expected,(command,result.returncode,result.stderr)
    return result

pc('init','--initial-branch=main',str(seed))
pc('config','user.name','Fixture author',cwd=seed)
pc('config','user.email','fixture@example.invalid',cwd=seed)
(seed/'readme.txt').write_bytes(b'Initial revision\n')
# A deterministic, poorly compressible file ensures interruption hits a pack.
(seed/'payload.bin').write_bytes(b''.join(hashlib.sha256(str(n).encode()).digest() for n in range(8192)))
pc('add','-A',cwd=seed);pc('commit','-m','Initial PC revision',cwd=seed)
initial=pc('rev-parse','HEAD',cwd=seed).stdout.strip()
pc('init','--bare','--initial-branch=main',str(remote))
# Seed loose objects without Git's local transport invoking an MSYS shell.
# All subsequent remote exchanges use real HTTPS upload/receive-pack.
shutil.copytree(seed/'.git/objects',remote/'objects',dirs_exist_ok=True)
pc('update-ref','refs/heads/main',initial,cwd=remote)
pc('config','http.receivepack','true',cwd=remote)
(volume/'credentials').write_bytes(b'test\nfixture-token\n')
(volume/'wrong-credentials').write_bytes(b'test\nwrong-token\n')
fixture=Fixture(case,scratch)
auth='Authorization: Basic '+base64.b64encode(b'test:fixture-token').decode()
pc('remote','add','origin',fixture.url,cwd=seed)
pc('config','http.extraHeader',auth,cwd=seed)
def memory_check(result):
    stats=re.findall(r'TAB5_GIT_MEMORY (\d+) (\d+)',result.stderr)
    assert stats and all(int(current)==0 for current,_ in stats),(result.returncode,result.stderr)
    return max(int(peak) for _,peak in stats)
def native(name,*command,cwd='/',expected=0,env=None):
    environment=dict(base_env,TAB5_GIT_TEST_CA=str(scratch/'ca.pem'))
    if env:environment.update(env)
    before=len(fixture.requests);started=time.monotonic()
    result=subprocess.run([str(args.client.resolve()),volume.as_posix(),cwd,'git',*command],
                          capture_output=True,text=True,encoding='utf-8',env=environment,timeout=35)
    peak=memory_check(result)
    assert result.returncode==expected,(name,result.returncode,result.stdout,result.stderr)
    assert 'fixture-token' not in result.stdout+result.stderr
    checks.append({'name':name,'exitCode':result.returncode,'elapsedSeconds':round(time.monotonic()-started,3),
                   'retainedBytes':0,'peakTrackedBytes':peak,'httpRequests':len(fixture.requests)-before})
    print(name,'PASS',flush=True)
    return result
def oid(repo,reference='HEAD'):
    return pc('rev-parse',reference,cwd=repo).stdout.strip()
try:
    # The server itself must first interoperate with installed Git and verify TLS.
    pc('-c','http.extraHeader='+auth,'clone',fixture.url,str(case/'pc-check'))
    assert oid(case/'pc-check')==initial
    pc('fsck','--strict',cwd=case/'pc-check')
    print('PC Git smart HTTPS fixture PASS',flush=True)
    if args.pc_only:
        print(json.dumps({'passed':True,'scope':'PC Git fixture only','httpRequests':len(fixture.requests)}))
    else:
        native('HTTPS clone with trusted CA and Basic credentials','clone','--depth','0','--credential-file','/credentials',fixture.url,'/project')
        project=volume/'project';assert oid(project)==initial
        assert (project/'payload.bin').read_bytes()==(seed/'payload.bin').read_bytes()
        pc('fsck','--strict',cwd=project)
        native('Configure author','config','user.name','Tab5 author',cwd='/project')
        native('Configure email','config','user.email','tab5@example.invalid',cwd='/project')
        (seed/'readme.txt').write_bytes(b'Second PC revision\n')
        pc('add','-A',cwd=seed);pc('commit','-m','Second PC revision',cwd=seed);pc('push','origin','main',cwd=seed)
        second=oid(seed)
        native('HTTPS fetch preserves working HEAD','fetch',cwd='/project')
        assert oid(project)==initial and oid(project,'refs/remotes/origin/main')==second
        native('HTTPS fast-forward pull','pull','--ff-only',cwd='/project')
        assert oid(project)==second and (project/'readme.txt').read_bytes()==b'Second PC revision\n'
        (project/'native.txt').write_bytes(b'Native command revision\n')
        native('Stage native revision','add','-A',cwd='/project')
        native('Commit native revision','commit','-m','Native HTTPS revision',cwd='/project')
        native_head=oid(project)
        native('HTTPS push interoperates with PC Git','push',cwd='/project')
        assert oid(remote,'refs/heads/main')==native_head
        pc('fsck','--strict',cwd=remote)
        pc('-c','http.extraHeader='+auth,'fetch',fixture.url,'main',cwd=case/'pc-check')
        assert oid(case/'pc-check','FETCH_HEAD')==native_head

        # A later server commit makes subsequent network downloads meaningful.
        pc('fetch','origin',cwd=seed);pc('reset','--hard','origin/main',cwd=seed)
        (seed/'later.bin').write_bytes(b''.join(hashlib.sha256(('later'+str(n)).encode()).digest() for n in range(2048)))
        pc('add','-A',cwd=seed);pc('commit','-m','Later server revision',cwd=seed);pc('push','origin','main',cwd=seed)
        server_head=oid(remote)
        before_tracking=oid(project,'refs/remotes/origin/main')
        fixture.disconnect_upload_pack=True
        native('Interrupted pack download retains old references','fetch',cwd='/project',expected=1)
        assert fixture.interruptions and oid(project)==native_head
        assert oid(project,'refs/remotes/origin/main')==before_tracking
        fixture.disconnect_upload_pack=False
        pc('fsck','--strict',cwd=project)
        native('Retry fetch after interruption succeeds','fetch',cwd='/project')
        assert oid(project,'refs/remotes/origin/main')==server_head

        (project/'native.txt').write_bytes(b'Local divergent revision\n')
        native('Stage divergent revision','add','-A',cwd='/project')
        native('Commit divergent revision','commit','-m','Divergent local revision',cwd='/project')
        divergent=oid(project)
        native('Non-fast-forward push refused without changing either HEAD','push',cwd='/project',expected=1)
        assert oid(project)==divergent and oid(remote)==server_head
        for name,url,ca in [
            ('Unknown CA rejected',fixture.url,str(wrong/'ca.pem')),
            ('Hostname mismatch rejected',fixture.url.replace('localhost','127.0.0.1'),str(scratch/'ca.pem')),
        ]:
            before=len(fixture.requests)
            destination='/bad-ca' if name.startswith('Unknown') else '/bad-host'
            native(name,'clone','--credential-file','/credentials',url,destination,expected=1,
                   env={'TAB5_GIT_TEST_CA':ca,'GIT_SSL_CAINFO':ca})
            assert len(fixture.requests)==before,'TLS verification must precede any HTTP authentication'
            assert not (volume/destination[1:]).exists()
        native('Bad credentials refused','clone','--credential-file','/wrong-credentials',fixture.url,'/bad-auth',expected=1)
        assert not (volume/'bad-auth').exists()
        fixture.delay=3
        started=time.monotonic()
        native('Cancellation while HTTPS peer delays response','fetch',cwd='/project',expected=130,
               env={'TAB5_GIT_CANCEL_MS':'2600'})
        elapsed=time.monotonic()-started
        assert checks[-1]['httpRequests']>=2,'Cancellation must reach the authenticated HTTP request'
        assert elapsed<8,(elapsed,'cancellation remained blocked')
        fixture.delay=0
        assert oid(project)==divergent and oid(remote)==server_head
        pc('fsck','--strict',cwd=project)
        for name,expected_hash in source_hashes.items():
            assert hashlib.sha256((release/name).read_bytes()).hexdigest()==expected_hash,('Source changed during test',name)
        assert hashlib.sha256(args.client.read_bytes()).hexdigest()==client_hash,'Client changed during test'
        report={'passed':True,'version':'0.8.25','physicalDeviceTested':False,
                'method':'Production Git command wrapper with libgit2 mbedTLS over loopback TLS to installed Git http-backend; real clone/fetch/pull/push and independent PC Git fsck.',
                'checks':checks,'interruptedPackResponses':fixture.interruptions,'tlsNames':['localhost'],
                'sourceHashes':source_hashes,'clientSha256':client_hash,'buildEvidence':build_evidence,
                'limitations':['Host allocator metrics do not measure ESP32 SRAM or actual task stack','Test CA and private keys exist only outside the release','OS network gate is replaced by host adapter; target integration compiled separately']}
        (release/'tools/verification/git-https-0825.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({'passed':True,'checks':len(checks)}),flush=True)
finally:
    fixture.close()
