#!/usr/bin/env python3
"""Execute actual main SD recovery/eject/probe with pinned Arduino File/VFS, production volume/lease code and Map close."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,zipfile,sys,importlib.util
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path);p.add_argument('--build',type=Path);p.add_argument('--probe-race-before-fix',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parents[1];s=next(r.glob('Tab5OS_Arduino_*_Ecriture'));sdkfixture=here.parent/'storage-sdk-tests-0845';out=(a.build or r.parents[1]/'build_0845/sd-recovery').resolve();out.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def method(src,name):
 begin=src.rindex(name);brace=src.index('{',begin);depth=0
 for end in range(brace,len(src)):
  if src[end]=='{':depth+=1
  elif src[end]=='}':
   depth-=1
   if not depth:return src[begin:end+1]
 raise AssertionError(name)
main=next(s.glob('*.ino'));files=['Tab5VolumeFS.cpp','Tab5VolumeFS.h','Tab5Storage.cpp','Tab5Storage.h','Tab5SystemPath.h','Tab5Map.cpp']
sourceHashes={str((s/n).relative_to(r)):sha(s/n) for n in files};sourceHashes[str(main.relative_to(r))]=sha(main)
used=[*here.rglob('*'),sdkfixture/'Arduino.h',sdkfixture/'Tab5Serial.h',sdkfixture/'Tab5UsbStorage.h',*sdkfixture.joinpath('freertos').rglob('*'),*sdkfixture.joinpath('sdk').rglob('*')]
patcher=r/'tools/build_linux/prepare_fs_errno.py'
if patcher.exists():used.append(patcher)
def fixture_hashes():return {str(f.relative_to(r)):sha(f) for f in used if f.is_file() and '__pycache__' not in f.parts}
fixtureHashes=fixture_hashes()

names=['static bool probeSdWrite(', 'static void serviceSdRecovery(', 'static const char* storageSdAction(']
def run(label,source,sketchMain,fixed):
 b=out/label;b.mkdir(exist_ok=True)
 for f in sdkfixture.rglob('*'):
  if f.is_file() and '__pycache__' not in f.parts and f.suffix in ['.h','.cpp']:
   target=b/f.relative_to(sdkfixture);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,target)
 shutil.copyfile(here/'host_vfs.cpp',b/'host_vfs.cpp')
 for n in files[:-1]:shutil.copyfile(source/n,b/n)
 # Exact SDK input is retained; production's isolated backport is applied only
 # to the current build, as it is in the firmware build environment.
 patcher=r/'tools/build_linux/prepare_fs_errno.py'
 if fixed and patcher.exists():
  spec=importlib.util.spec_from_file_location('fs_errno',patcher);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
  vfs=b/'sdk/vfs_api.cpp';vfs.write_bytes(mod.patch_vfs(vfs.read_bytes()))
 src=sketchMain.read_text();body='\n\n'.join(method(src,n) for n in names)
 close=method((source/'Tab5Map.cpp').read_text(),'void Tab5MapEngine::closeOfflinePack(')
 fixture=(here/'main.cpp').read_text().replace('// MAP_CLOSE',close).replace('// PRODUCTION_MAIN',body)
 (b/'main.cpp').write_text(fixture)
 common=['g++','-std=c++17','-fpermissive','-O1','-g','-pthread','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(b),'-I'+str(b/'sdk')]+(['-DEXPECT_FIXED'] if fixed else [])+(['-DEXPECT_RACE_BUG'] if fixed and a.probe_race_before_fix else [])+(['-DOWNED_PROBE_RESULT'] if fixed and not a.probe_race_before_fix else [])
 objects=[]
 for n in ['sdk/vfs_api.cpp','sdk/FS.cpp','Tab5VolumeFS.cpp','Tab5Storage.cpp','main.cpp','host_vfs.cpp']:
  obj=b/(Path(n).stem+'.o');cmd=common+['-c',str(b/n),'-o',str(obj)]
  result=subprocess.run(cmd,capture_output=True,text=True);(b/(Path(n).stem+'-build.log')).write_text(result.stdout+result.stderr)
  if result.returncode:raise RuntimeError(result.stdout+result.stderr)
  objects.append(obj)
 cmd=common+[str(x) for x in objects]+['-Wl,--wrap=malloc,--wrap=fopen,--wrap=stat,--wrap=opendir,--wrap=mkdir,--wrap=rename,--wrap=unlink,--wrap=rmdir,--wrap=fflush,--wrap=fsync,--wrap=fclose,--wrap=_Z16tab5StorageFaultPKcS0_i','-o',str(b/'test')]
 result=subprocess.run(cmd,capture_output=True,text=True)
 if result.returncode:raise RuntimeError(result.stdout+result.stderr)
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 result=subprocess.run([str(b/'test'),str(b/'sd')],capture_output=True,text=True,env=env);(b/'run.log').write_text(result.stdout+result.stderr)
 print(label,result.stdout+result.stderr)
 if result.returncode:raise RuntimeError(result.stdout+result.stderr)
 return json.loads(result.stdout)
baseline=None
if a.baseline_zip:
 b=out/'baseline-source';b.mkdir(exist_ok=True)
 with zipfile.ZipFile(a.baseline_zip) as z:
  for n in files:
   name=next(x for x in z.namelist() if x.endswith('/'+n) and '/Tab5OS_Arduino_' in x and '/tools/' not in x);(b/n).write_bytes(z.read(name))
  name=next(x for x in z.namelist() if x.endswith('.ino') and '/Tab5OS_Arduino_' in x);(b/'main.ino').write_bytes(z.read(name))
 baseline=run('baseline',b,b/'main.ino',False)
current=run('before-owned-result' if a.probe_race_before_fix else 'current',s,here/'before-owned-result.inc' if a.probe_race_before_fix else main,True)
if a.probe_race_before_fix:raise SystemExit(0)
assert sourceHashes=={**{str((s/n).relative_to(r)):sha(s/n) for n in files},str(main.relative_to(r)):sha(main)},'sources changed during tests'
assert fixtureHashes==fixture_hashes(),'test or SDK fixture changed during tests'
report={'version':'0.8.45','passed':True,'physicalDeviceTested':False,'baseline':baseline,'current':current,'sourceHashes':sourceHashes,'testSourceHashes':fixtureHashes,'method':__doc__,'mainMethods':{n:hashlib.sha256(method(main.read_text(),n).encode()).hexdigest() for n in names},'limitations':['Entire actual serviceSdRecovery/storageSdAction/probeSdWrite functions execute, with actual SDK FS/VFS and production VolumeFS/Storage/Map.closeOfflinePack.','Application state, SD.begin/end and GPIO/SPI are adapters; SD ownership/exclusive/refcounts are production code. No physical remount, bus/timing, FatFS or electrical result is claimed.','POSIX flush/sync/close results are injected on real host files. The actual SDK close/flush calling order is executed.','ASan/UBSan enabled; LSan unavailable, explicit file references checked.']}
evidence=r/'tools/verification/sd-recovery-0845';evidence.mkdir(exist_ok=True)
for label in (['baseline','current'] if a.baseline_zip else ['current']):
 for f in (out/label).glob('*.log'):shutil.copyfile(f,evidence/(label+'-'+f.name))
report['evidenceHashes']={str(f.relative_to(r)):sha(f) for f in evidence.glob('*.log')}
(r/'tools/verification/sd-recovery-0845.json').write_text(json.dumps(report,indent=2)+'\n')
