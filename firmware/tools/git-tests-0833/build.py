"""Compile production Git and vendored libgit2 for real loopback HTTPS tests.
Host adaptations: 64-bit target, OpenSSL instead of ESP mbedTLS and aligned
SHA1 input as used on RISC-V. No mock repositories or command implementation.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,shutil,subprocess,tempfile
here=Path(__file__).resolve().parent;release=here.parent.parent
parser=argparse.ArgumentParser();parser.add_argument('--sketch',type=Path);parser.add_argument('--build',type=Path);parser.add_argument('--baseline',action='store_true');args=parser.parse_args()
sketch=(args.sketch or release/'Tab5OS_Arduino_v0.8.33_Ecriture').resolve()
build=(args.build or Path(tempfile.mkdtemp(prefix='tab5-git-engine-0833-'))).resolve();build.mkdir(parents=True,exist_ok=True)
vendor=sketch/'src/tab5git';host=build/'vendor';shutil.copytree(vendor,host,dirs_exist_ok=True)
feature=host/'src/util/git2_features.h';text=feature.read_text()
replacements={'#define GIT_ARCH_32 1':'#define GIT_ARCH_64 1','#define GIT_MBEDTLS 1':'#define GIT_OPENSSL 1','#define GIT_SHA256_MBEDTLS 1':'#define GIT_SHA256_OPENSSL 1'}
for before,after in replacements.items():
 assert text.count(before)==1; text=text.replace(before,after)
feature.write_text(text)
sha1=host/'src/util/hash/sha1dc/sha1.c'
text=sha1.read_text();text=text.replace('#if defined(SHA1DC_FORCE_UNALIGNED_ACCESS) || defined(SHA1DC_ON_INTEL_LIKE_PROCESSOR)','#if 0 /* host sanitizer: match aligned RISC-V input path */');sha1.write_text(text)
inputs=[sketch/p for p in ['Tab5Git.cpp','Tab5Git.h','Tab5GitPort.cpp','Tab5GitPort.h','Tab5GitPull.cpp','Tab5GitPull.h','Tab5GitJournal.h','Tab5TerminalPath.h','Tab5FilePolicy.h']]+sorted(p for p in vendor.rglob('*') if p.is_file())+[here/p for p in ['build.py','cli.cpp','sha256_host.c','baseline_0832.cpp']]
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
client=build/('git_cli_baseline_0832' if args.baseline else 'git_cli')
client_source=here/'baseline_0832.cpp' if args.baseline else sketch/'Tab5Git.cpp'
subprocess.run(['g++','-std=c++17',*flags,'-no-pie','-DTAB5_GIT_HOST','-I'+str(sketch),str(here/'cli.cpp'),str(client_source),str(sketch/'Tab5GitPort.cpp'),str(sketch/'Tab5GitPull.cpp'),*[str(p) for p in objects],'-lssl','-lcrypto','-o',str(client)],check=True)
print('Compiled production Git and',len(sources)-1,'vendor C units',flush=True)

client.chmod(0o755)
print(client)

manifest={'version':'0.8.33','passed':True,'sourceHashes':snapshot,'hostFeatureChanges':replacements,'hostSha1Adaptation':'Aligned input path matching RISC-V, rather than x86 unaligned loads','vendorTranslationUnits':len(sources)-1,'clientSha256':digest(client)}
(client.with_suffix('.json')).write_text(json.dumps(manifest,indent=2)+'\n')
