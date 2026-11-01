#!/usr/bin/env python3
"""Execute production VolumeFS, storage leases/recovery and atomic writes over failure-injected host file boundaries."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path);p.add_argument('--build',type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;s=next(r.glob('Tab5OS_Arduino_v*_Ecriture'));out=(a.build or r.parent.parent/'build_0845/storage-policy').resolve();out.mkdir(parents=True,exist_ok=True)
files=['Tab5VolumeFS.cpp','Tab5VolumeFS.h','Tab5Storage.cpp','Tab5Storage.h','Tab5SystemPath.h','Tab5AtomicFile.h']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
start={str((s/n).relative_to(r)):sha(s/n) for n in files}
def run(label,source,fixed):
 b=out/label;b.mkdir(exist_ok=True)
 for old in ['atomic','volume']:
  if (b/old).is_file():(b/old).unlink()
 for n in ['Arduino.h','FS.h','vfs_api.h','Tab5UsbStorage.h','Tab5Serial.h','AtomicPlatform.h','main.cpp','atomic.cpp']:shutil.copy2(here/n,b/n)
 for n in files:shutil.copy2(source/n,b/n)
 atomic=(b/'Tab5AtomicFile.h').read_text()
 atomic='\n'.join(line for line in atomic.splitlines() if line not in ['#include "Tab5VolumeFS.h"','#include <SD.h>'])+'\n'
 (b/'AtomicProduction.h').write_text(atomic)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
 result={}
 for kind,units in [('volume',['Tab5VolumeFS.cpp','Tab5Storage.cpp','main.cpp']),('atomic',['Tab5Storage.cpp','atomic.cpp'])]:
  cmd=['g++','-std=c++17','-pthread','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I'+str(b)]+(['-DEXPECT_FIXED'] if fixed else [])+[str(b/n) for n in units]+['-o',str(b/(kind+'-test'))]
  c=subprocess.run(cmd,capture_output=True,text=True)
  if c.returncode:raise RuntimeError(c.stdout[-4000:]+c.stderr[-4000:])
  x=subprocess.run([str(b/(kind+'-test'))],env=env,capture_output=True,text=True);(b/(kind+'.log')).write_text(x.stdout+x.stderr)
  if x.returncode:raise RuntimeError(x.stdout+x.stderr)
  result[kind]=json.loads(x.stdout);print(label,kind,x.stdout.strip())
 return result
baseline=None
if a.baseline_zip:
 b=out/'baseline-source';b.mkdir(exist_ok=True)
 for old in ['atomic','volume']:
  if (b/old).is_file():(b/old).unlink()
 with zipfile.ZipFile(a.baseline_zip) as z:
  for n in files:
   found=[x for x in z.namelist() if x.endswith('/'+n) and '/Tab5OS_Arduino_' in x and '/tools/' not in x];assert len(found)==1,(n,found)
   (b/n).write_bytes(z.read(found[0]))
 baseline=run('baseline',b,False)
current=run('current',s,True)
assert start=={str((s/n).relative_to(r)):sha(s/n) for n in files}
report={'version':'0.8.45','passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':True,'sourceHashes':start,'testSourceHashes':{str(x.relative_to(r)):sha(x) for x in here.iterdir() if x.is_file()},'baseline':baseline,'current':current,'method':__doc__,'limitations':['Arduino FS/VFS/USB file operations are injected boundaries, not actual SD/FatFS/SPI.','No physical remount, media disappearance, power loss or electrical diagnosis claimed.','LSan unavailable in this runner; explicit volume reference checks complement ASan/UBSan. Host tests execute production wrappers/recovery and unchanged atomic helper functions (include substitution only).']}
(r/'tools/verification/storage-policy-0845.json').write_text(json.dumps(report,indent=2)+'\n')
