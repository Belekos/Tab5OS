#!/usr/bin/env python3
"""Measure the list object's P4 ABI size using the actual firmware compiler."""
from pathlib import Path
import hashlib,json,subprocess,zipfile
r=Path(__file__).resolve().parents[2];w=r.parent.parent;s=r/'Tab5OS_Arduino_v0.8.47_Ecriture'
db=json.loads((w/'build_0847/target-build/compile_commands.json').read_text())
entry=next(x for x in db if Path(x['file']).name=='Tab5MediaList.cpp')
archive=w/'baseline_0846/Tab5OS_0.8.46_Reseau_Memoire.zip'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(archive)=='22d99628cc3ab6a9673be1886755ec9a3092aaede3f017a7e0e20443cc0816a6'
sizes={};hashes={}
for version in ['baseline','current']:
 b=w/'build_0847/media-ui-size'/version;b.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(archive) as z:
  for name in ['Tab5MediaList.h','WriterKeys.h','Tab5FileSort.h']:
   (b/name).write_bytes((s/name).read_bytes() if version=='current' else z.read('Tab5OS_Ecriture_v0.8.46/Tab5OS_Arduino_v0.8.46_Ecriture/'+name))
 source=b/'size.cpp';source.write_text('#include "Tab5MediaList.h"\nextern "C" { unsigned char tab5_media_list_object_size[sizeof(Tab5MediaList)]; }\n')
 command=[str(source) if x==entry['file'] else x for x in entry['arguments']]
 obj=b/'size.o';command[command.index('-o')+1]=str(obj)
 cp=subprocess.run(command,cwd=entry['directory'],capture_output=True,text=True);(b/'compile.log').write_text(cp.stdout+cp.stderr)
 if cp.returncode:raise RuntimeError(cp.stderr)
 nm=Path(command[0]).with_name('riscv32-esp-elf-nm')
 text=subprocess.check_output([str(nm),'-S',str(obj)],text=True)
 line=next(x for x in text.splitlines() if x.endswith(' tab5_media_list_object_size'))
 sizes[version]=int(line.split()[1],16);hashes[version]=sha(b/'Tab5MediaList.h')
report={'version':'0.8.47','passed':True,'physicalDeviceTested':False,'method':'sizeof(Tab5MediaList) compiled to BSS probe, size read with exact P4 toolchain nm; not linked into firmware.','objectBytes':sizes,'headerSha256':hashes,'compiler':subprocess.check_output([command[0],'--version'],text=True).splitlines()[0]}
p=r/'tools/verification/media-ui-size-0847.json';p.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
