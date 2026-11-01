#!/usr/bin/env python3
"""Run actual HTTP code with legacy network adapter, real POSIX files and I/O faults."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile,zipfile
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.42_Ecriture';here=Path(__file__).resolve().parent;legacy=root/'tools/network-tests'
p=argparse.ArgumentParser();p.add_argument('--baseline-zip');args=p.parse_args()
source=(sketch/'Tab5NetworkHttp.cpp').read_text()
if args.baseline_zip:
 with zipfile.ZipFile(args.baseline_zip) as z:source=z.read(next(n for n in z.namelist() if n.endswith('/Tab5NetworkHttp.cpp'))).decode()
build=Path(tempfile.mkdtemp(prefix='tab5-http-0842-'))
platform=(legacy/'HttpPlatform.h').read_text().replace('#include <io.h>','#include <unistd.h>\n#include <cerrno>')
platform=platform.replace('using ssize_t=long long;','').replace('flags|_O_BINARY','flags').replace('::_open(','::open(').replace('::_close(','::close(').replace('::_write(','::write(').replace('::_access(','::access(').replace('::_unlink(','::unlink(').replace('#define fsync _commit','')
platform=platform.replace('inline uint32_t tick=','inline int failStat=0,failAccess=0;\ninline std::string faultPath;\ninline uint32_t tick=')
platform=platform.replace('inline void reset(){','inline void reset(){failStat=failAccess=0;faultPath.clear();')
platform=platform.replace('inline int fakeFileAccess(const char* p,int mode){','inline int fakeFileAccess(const char* p,int mode){if(HttpFake::faultPath==p&&HttpFake::failAccess){--HttpFake::failAccess;errno=EIO;return -1;}')
platform=platform.replace('inline int fakeFileStat(const char* p,struct stat* out){','inline int fakeFileStat(const char* p,struct stat* out){if(HttpFake::faultPath==p&&HttpFake::failStat){--HttpFake::failStat;errno=EIO;return -1;}')
(build/'HttpPlatform.h').write_text(platform)
for header in ['Tab5SecureClient.h','Tab5CooperativeClient.h','Tab5NetworkGate.h','Tab5Storage.h','Tab5VolumeFS.h','Tab5UsbStorage.h','HTTPClient.h','NetworkClient.h']:(build/header).write_text('#include "HttpPlatform.h"\n')
(build/'Tab5NetworkHttp.cpp').write_text(source.replace('::write(fd,','::fakeFileWrite(fd,'))
fixture=(legacy/'http.cpp').read_text()
extra=r'''
 // A transient metadata failure must never be treated as consent to overwrite.
 auto put=[](const char* path,const char* text){std::ofstream f(HttpFake::root/path,std::ios::binary);f<<text;};
 put("file","protected");HttpFake::reset();HttpFake::faultPath="/sd/file";HttpFake::failStat=1;HttpFake::failAccess=1;
 HttpFake::replies.push_back({200,"unexpected overwrite"});int metadataRc=run({"wget","-O","sd:/file","https://example.org"});
 bool metadataProtected=metadataRc==1&&get("file")=="protected";
 put("file","protected");HttpFake::reset();HttpFake::faultPath="/sd/file";HttpFake::failAccess=1;
 HttpFake::replies.push_back({200,"new"});int finalRc=run({"wget","--overwrite","-O","sd:/file","https://example.org"});
 bool finalProtected=finalRc==1&&get("file")=="protected";
 put("file","protected");put("file.tab5-download-previous","recovery-copy");HttpFake::reset();HttpFake::faultPath="/sd/file.tab5-download-previous";HttpFake::failAccess=1;
 HttpFake::replies.push_back({200,"new"});int backupRc=run({"wget","--overwrite","-O","sd:/file","https://example.org"});
 bool backupProtected=backupRc==1&&get("file")=="protected"&&get("file.tab5-download-previous")=="recovery-copy";
#ifndef BASELINE
 assert(metadataProtected&&finalProtected&&backupProtected);
#endif
 std::cout<<"{\"existingRegressionCasesPassed\":true,\"initialMetadataFaultProtected\":"<<(metadataProtected?"true":"false")<<",\"finalMetadataFaultProtected\":"<<(finalProtected?"true":"false")<<",\"backupMetadataFaultProtected\":"<<(backupProtected?"true":"false")<<"}"<<std::endl;
'''
fixture=fixture.rsplit('}',1)[0]+extra+'}\n';(build/'http.cpp').write_text(fixture)
cmd=['g++','-std=c++17','-O1','-g','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-I'+str(build),'-I'+str(sketch),str(build/'http.cpp'),'-o',str(build/'http')]
if args.baseline_zip:cmd.insert(1,'-DBASELINE')
subprocess.run(cmd,check=True)
run=subprocess.run([str(build/'http'),str(build/'volume')],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));print(run.stdout+run.stderr,end='');run.check_returncode()
result=json.loads(run.stdout.strip().splitlines()[-1]);report={'version':'0.8.41' if args.baseline_zip else '0.8.42',**result,'sourceSha256':hashlib.sha256(source.encode()).hexdigest(),'scope':'Actual HTTP transfer implementation with fake HTTP responses and real host POSIX filesystem; initial/final/backup EIO injection plus historical truncation, write failure, cancellation, HTTPS downgrade, redirect, deadline and FD cleanup cases. ASan+UBSan. No radio/TLS/SD hardware executed.'}
out=root/'tools/audit-0842'/('http-before.json' if args.baseline_zip else 'http-after.json');out.write_text(json.dumps(report,indent=2)+'\n');print(out)
