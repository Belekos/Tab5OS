from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from test_report_0842 import save_report
import argparse,hashlib,json,shutil,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path);p.add_argument('--json-include',type=Path);p.add_argument('--build',type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parent.parent;workspace=release.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.42_Ecriture';build=a.build or workspace/'build_0842/atomic-host';build.mkdir(parents=True,exist_ok=True)
include=a.json_include or workspace/'build_0842/sketchbook/libraries/ArduinoJson/src'
names=['Tab5AtomicFile.h','Tab5JsonFile.h','Tab5FilePolicy.h','Tab5HiddenFiles.h','Tab5Tools.cpp'];snapshot={}
for n in names:
 if a.baseline_zip:
  with zipfile.ZipFile(a.baseline_zip) as z:
   matches=[p for p in z.namelist() if p.endswith('/'+n) and '/tools/' not in p];assert len(matches)==1;source=z.read(matches[0])
 else:source=(sketch/n).read_bytes()
 snapshot[n]=hashlib.sha256(source).hexdigest();(build/n).write_bytes(source)
for n in ['Platform.h','test.cpp']:shutil.copy2(here/n,build/n)
for n in ['Arduino.h','Tab5VolumeFS.h','SD.h','FS.h','Tab5Storage.h','Tab5UsbStorage.h']:(build/n).write_text('#include "Platform.h"\n')
(build/'Tab5Json.h').write_text('#include <ArduinoJson.h>\ninline ArduinoJson::Allocator* tab5JsonAllocator(){return nullptr;}\n')
source=(build/'Tab5Tools.cpp').read_text();start=source.index('bool readJson(');end=source.index('\n',start);(build/'read-json.inc').write_text(source[start:end]+'\n')
cmd=['g++','-std=c++17','-O2','-I'+str(build),'-I'+str(include),str(build/'test.cpp'),'-o',str(build/'test')]
r=subprocess.run(cmd,capture_output=True,text=True);print(r.stdout+r.stderr,end='');r.check_returncode()
r=subprocess.run([str(build/'test')]+(['baseline'] if a.baseline_zip else []),capture_output=True,text=True,timeout=30);print(r.stdout+r.stderr,end='');r.check_returncode()
report=dict(version='0.8.42',baseline=bool(a.baseline_zip),physicalDeviceTested=False,sources=snapshot,results=json.loads(r.stdout),method='Production AtomicFile/HiddenFiles helpers and production Tools readJson method extracted verbatim, real ArduinoJson 7, fault-injected filesystem. No durability or SD-hardware claim.')
save_report(report,(release/'tools/verification'/('atomic-storage-0842-baseline.json' if a.baseline_zip else 'atomic-storage-0842.json')),here,r.stdout+r.stderr)
