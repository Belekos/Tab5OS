"""Execute production volume lifetime/recovery ownership using the existing host boundary adapter."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from test_report_0842 import save_report
import hashlib,json,shutil,subprocess
here=Path(__file__).resolve().parent;r=here.parent.parent;s=r/'Tab5OS_Arduino_v0.8.42_Ecriture';old=r/'tools/system-resources-tests';build=r.parent.parent/'build_0842/volume-host';build.mkdir(parents=True,exist_ok=True)
for folder in [old,old/'volume']:
 for p in folder.iterdir():
  if p.suffix in ('.cpp','.h'):shutil.copy2(p,build/p.name)
names=['Tab5VolumeFS.h','Tab5VolumeFS.cpp','Tab5Storage.h','Tab5Storage.cpp','Tab5SystemPath.h']
for n in names:shutil.copy2(s/n,build/n)
(build/'Tab5Serial.h').write_text('// Native test: all production serial use is inside ARDUINO guards.\n')
cmd=['g++','-std=c++17','-pthread','-I'+str(build),*[str(build/n) for n in ['Tab5VolumeFS.cpp','Tab5Storage.cpp','main.cpp']],'-o',str(build/'test')];subprocess.run(cmd,check=True);out=subprocess.check_output([str(build/'test')],text=True);print(out,end='')
report=dict(version='0.8.42',passed=True,physicalDeviceTested=False,sourceHashes={s.name+'/'+n:hashlib.sha256((s/n).read_bytes()).hexdigest() for n in names},output=out,method=__doc__,limitations=['VFS hardware adapter simulated; actual FileImpl wrappers and Storage leases execute.','No physical media recovery, electrical or power-loss claim.'])
save_report(report,r/'tools/verification/storage-volume-0842.json',here,out)
