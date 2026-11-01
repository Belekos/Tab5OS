#!/usr/bin/env python3
"""Use unmodified pinned Arduino File/VFS implementations, not an FS open-success stub."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path);p.add_argument('--build',type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;s=next(r.glob('Tab5OS_Arduino_v*_Ecriture'));out=(a.build or r.parent.parent/'build_0845/storage-sdk').resolve();out.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def method(src,name):
 begin=src.index(name);brace=src.index('{',begin);depth=0
 for end in range(brace,len(src)):
  if src[end]=='{':depth+=1
  elif src[end]=='}':
   depth-=1
   if not depth:return src[begin:end+1]
 raise AssertionError(name)
files=['Tab5VolumeFS.cpp','Tab5VolumeFS.h','Tab5Storage.cpp','Tab5Storage.h','Tab5SystemPath.h','Tab5Map.cpp']
sourceHashes={str((s/n).relative_to(r)):sha(s/n) for n in files}
patch_script=r/'tools/build_linux/prepare_fs_errno.py';sourceHashes[str(patch_script.relative_to(r))]=sha(patch_script)
spec=importlib.util.spec_from_file_location('patch',patch_script);patch=importlib.util.module_from_spec(spec);spec.loader.exec_module(patch)
def run(label,source,fixed):
 b=out/label;b.mkdir(exist_ok=True)
 for f in here.rglob('*'):
  if f.is_file() and '__pycache__' not in f.parts and f.suffix in ['.h','.cpp']:
   target=b/f.relative_to(here);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,target)
 for n in files[:-1]:shutil.copy2(source/n,b/n)
 if fixed:(b/'sdk/vfs_api.cpp').write_bytes(patch.patch_vfs((here/'sdk/vfs_api.cpp').read_bytes()))
 body=method((source/'Tab5Map.cpp').read_text(),'void Tab5MapEngine::closeOfflinePack(')
 (b/'main.cpp').write_text((here/'main.cpp').read_text().replace('// MAP_CLOSE',body))
 common=['g++','-std=c++17','-fpermissive','-O1','-g','-pthread','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(b),'-I'+str(b/'sdk')]+(['-DEXPECT_FIXED'] if fixed else [])
 objects=[]
 for n in ['sdk/vfs_api.cpp','sdk/FS.cpp','Tab5VolumeFS.cpp','Tab5Storage.cpp','main.cpp','host_vfs.cpp']:
  obj=b/(Path(n).stem+'.o');cmd=common+['-c',str(b/n),'-o',str(obj)]
  result=subprocess.run(cmd,capture_output=True,text=True)
  if result.returncode:raise RuntimeError(result.stdout+result.stderr)
  objects.append(obj)
 result=subprocess.run(common+[str(x) for x in objects]+['-Wl,--wrap=malloc,--wrap=fopen,--wrap=stat,--wrap=opendir,--wrap=mkdir,--wrap=rename,--wrap=unlink,--wrap=rmdir,--wrap=fflush,--wrap=fsync,--wrap=strdup','-o',str(b/'test')],capture_output=True,text=True)
 if result.returncode:raise RuntimeError(result.stdout+result.stderr)
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 result=subprocess.run([str(b/'test'),str(b/'sd')],capture_output=True,text=True,env=env);(b/'run.log').write_text(result.stdout+result.stderr)
 if result.returncode:raise RuntimeError(result.stdout+result.stderr)
 print(label,result.stdout.strip());return json.loads(result.stdout)
baseline=None
if a.baseline_zip:
 b=out/'baseline-source';b.mkdir(exist_ok=True)
 with zipfile.ZipFile(a.baseline_zip) as z:
  for n in files:
   name=next(x for x in z.namelist() if x.endswith('/'+n) and '/Tab5OS_Arduino_' in x and '/tools/' not in x)
   (b/n).write_bytes(z.read(name))
 baseline=run('baseline',b,False)
current=run('current',s,True)
assert sourceHashes=={**{str((s/n).relative_to(r)):sha(s/n) for n in files},str(patch_script.relative_to(r)):sha(patch_script)},'sources changed during tests'
report={'version':'0.8.45','passed':True,'physicalDeviceTested':False,'baseline':baseline,'current':current,'sourceHashes':sourceHashes,'testSourceHashes':{str(f.relative_to(r)):sha(f) for f in here.rglob('*') if f.is_file() and '__pycache__' not in f.parts},'currentSdkVfsSha256':hashlib.sha256(patch.patch_vfs((here/'sdk/vfs_api.cpp').read_bytes())).hexdigest(),'sdkSourceHashes':{str(f.relative_to(here)):sha(f) for f in (here/'sdk').iterdir() if f.is_file()},'methods':{'Map.closeOfflinePack':hashlib.sha256(method((s/'Tab5Map.cpp').read_text(),'void Tab5MapEngine::closeOfflinePack(').encode()).hexdigest()},'method':__doc__,'limitations':['Exact pinned Arduino FS.cpp/vfs_api.cpp and headers before; shipped isolated errno backport after; String, Stream, recursive mutex, USB and POSIX path/failure boundaries adapted. No FakeFS open result model.','Missing-path/descriptor-limit/malloc failures are injected on real VFS implementation, not physical FatFS or SD bus.','No claim of physical SD reconnect or specific device fault eliminated.','ASan/UBSan active; LSan disabled (process inspection unavailable), explicit volume references and host file closes checked.']}
(r/'tools/verification/storage-sdk-0845.json').write_text(json.dumps(report,indent=2)+'\n')
