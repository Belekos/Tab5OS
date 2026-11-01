#!/usr/bin/env python3
"""Test metadata-only isolated FS backport against the exact upstream implementation."""
from pathlib import Path
import hashlib,importlib.util,json,os,shutil,subprocess,argparse
p=argparse.ArgumentParser();p.add_argument('--build',type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;out=(a.build or r.parent.parent/'build_0845/fs-errno-host').resolve();out.mkdir(parents=True,exist_ok=True)
sketch=next(r.glob('Tab5OS_Arduino_v*_Ecriture'));atomic=sketch/'Tab5AtomicFile.h';atomic_source=atomic.read_text();start=atomic_source.index('inline bool tab5AtomicProbe(');atomic_probe=atomic_source[start:atomic_source.index('}',start)+1]
script=r/'tools/build_linux/prepare_fs_errno.py';spec=importlib.util.spec_from_file_location('errno_patch',script);patch=importlib.util.module_from_spec(spec);spec.loader.exec_module(patch)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
results={};patched=patch.patch_vfs((here/'sdk/vfs_api.cpp').read_bytes())
for label,fixed in [('baseline',False),('current',True)]:
 b=out/label;b.mkdir(exist_ok=True)
 for f in here.rglob('*'):
  if f.is_file() and '__pycache__' not in f.parts and f.suffix in ['.h','.cpp']:
   dest=b/f.relative_to(here);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
 if fixed:(b/'sdk/vfs_api.cpp').write_bytes(patched)
 (b/'errno.cpp').write_text((here/'errno.cpp').read_text().replace('// ATOMIC_PROBE',atomic_probe))
 flags=['g++','-std=c++17','-fpermissive','-O1','-g','-pthread','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(b),'-I'+str(b/'sdk')]+(['-DEXPECT_FIXED'] if fixed else [])
 cmd=flags+[str(b/n) for n in ['sdk/vfs_api.cpp','sdk/FS.cpp','errno.cpp','host_vfs.cpp']]+['-Wl,--wrap=malloc,--wrap=fopen,--wrap=stat,--wrap=opendir,--wrap=mkdir,--wrap=rename,--wrap=unlink,--wrap=rmdir,--wrap=fflush,--wrap=fsync,--wrap=strdup','-o',str(b/'test')]
 c=subprocess.run(cmd,capture_output=True,text=True);(b/'build.log').write_text(c.stdout+c.stderr)
 if c.returncode:raise RuntimeError(c.stdout+c.stderr)
 x=subprocess.run([str(b/'test'),str(b/'sd')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(b/'run.log').write_text(x.stdout+x.stderr)
 if x.returncode:raise RuntimeError(x.stdout+x.stderr)
 results[label]=json.loads(x.stdout);print(label,x.stdout.strip())
report={'version':'0.8.45','passed':True,'physicalDeviceTested':False,'atomicProbeSha256':hashlib.sha256(atomic_probe.encode()).hexdigest(),'method':__doc__,'sourceHashes':{str(script.relative_to(r)):sha(script),str(atomic.relative_to(r)):sha(atomic)},'testSourceHashes':{str(f.relative_to(r)):sha(f) for f in here.rglob('*') if f.is_file() and '__pycache__' not in f.parts},'upstreamVfsSha256':sha(here/'sdk/vfs_api.cpp'),'patchedVfsSha256':hashlib.sha256(patched).hexdigest(),**results,'limitations':['Real pinned FS.cpp/vfs_api.cpp with only current backport transformation. String/Stream/mutex and syscall/path/allocator boundaries adapted.','Malloc/strdup null with errno zero is injected to test a required local failure contract; not asserted observed on actual Tab5 allocator.','Negative fflush with errno zero is an explicit I/O failure and receives EIO only in that exact failure case.','No queue/timing/remount/layout/SDK installed file mutation. ASan/UBSan active; LSan unavailable.']}
(r/'tools/verification/fs-errno-0845.json').write_text(json.dumps(report,indent=2)+'\n')
