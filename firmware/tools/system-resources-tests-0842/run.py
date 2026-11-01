#!/usr/bin/env python3
"""Exercise real repair code and bundled zlib with corrupt streams and FS faults."""
import hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];s=r/'Tab5OS_Arduino_v0.8.42_Ecriture';h=Path(__file__).resolve().parent;out=r/'tools/verification/system-resources-0842';out.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
names=['Tab5SystemResources.cpp','Tab5SystemResources.h','Tab5SystemResourcesData.h','Tab5Storage.cpp','Tab5Storage.h'];before={str((s/n).relative_to(r)):sha(s/n) for n in names}
with tempfile.TemporaryDirectory(prefix='tab5-resources-0842-') as tmp:
 b=Path(tmp)
 for p in h.iterdir():
  if p.suffix in ('.h','.cpp'):shutil.copy2(p,b/p.name)
 for n in names:shutil.copy2(s/n,b/n)
 # Fault injection modifies generated data only in this test copy. Firmware
 # remains const; runtime implementation and the 40 compressed byte arrays match.
 p=b/'Tab5SystemResourcesData.h';p.write_text(p.read_text().replace('static const uint8_t','static uint8_t').replace('static const Tab5SystemResource','static Tab5SystemResource'))
 (b/'Tab5Serial.h').write_text('#pragma once\n')
 z=b/'src/tab5git/deps/zlib';shutil.copytree(s/'src/tab5git/deps/zlib',z)
 shutil.copy2(s/'src/tab5git/Tab5GitEmbeddedConfig.h',z.parent.parent/'Tab5GitEmbeddedConfig.h')
 flags=['-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-I.'];objects=[]
 def run(command,name):
  result=subprocess.run(command,cwd=b,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});(out/name).write_text(result.stdout+result.stderr)
  if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
  return result.stdout
 for name in ['adler32','crc32','inflate','inftrees','inffast','zutil']:
  obj=b/(name+'.o');run(['gcc','-std=c11',*flags,'-c',str(z/(name+'.c')),'-o',str(obj)],name+'-build.log');objects.append(str(obj))
 run(['g++','-std=c++17','-pthread',*flags,'-no-pie','main.cpp','Tab5Storage.cpp',*objects,'-o',str(b/'test')],'build.log')
 output=run([str(b/'test')],'run.log');print(output)
assert before=={str((s/n).relative_to(r)):sha(s/n) for n in names}
report={'version':'0.8.42','passed':True,'physicalDeviceTested':False,'sourceHashes':before,'testSourceHashes':{str(p.relative_to(r)):sha(p) for p in h.iterdir() if p.is_file()},'evidenceHashes':{str(p.relative_to(r)):sha(p) for p in out.iterdir() if p.is_file()},'result':output,'limitations':['Host storage and heap fault injection; SD hardware timing and power-loss behavior require device tests.','Bundled zlib compiled with ASan/UBSan; heap allocations tracked explicitly. LSan disabled because sandbox /proc task inspection is unavailable.']}
(r/'tools/verification/system-resources-0842.json').write_text(json.dumps(report,indent=2)+'\n')
