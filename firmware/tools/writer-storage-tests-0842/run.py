from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from test_report_0842 import save_report
import argparse,hashlib,json,shutil,subprocess,zipfile,os
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path);a=p.parse_args();here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.42_Ecriture';build=release.parent.parent/'build_0842/writer-storage-host';build.mkdir(parents=True,exist_ok=True)
names=['WriterStorage.cpp','WriterStorage.h','WriterWorkspace.cpp','WriterWorkspace.h','WriterDocument.cpp','WriterDocument.h','WriterEncoding.cpp','WriterEncoding.h','WriterSyntax.h'];sources={}
for n in names:
 if a.baseline_zip:
  with zipfile.ZipFile(a.baseline_zip) as z:
   path=next(p for p in z.namelist() if p.endswith('/'+n) and '/tools/' not in p);data=z.read(path)
 else:data=(sketch/n).read_bytes()
 sources[n]=hashlib.sha256(data).hexdigest();(build/n).write_bytes(data)
shutil.copy2(here/'test.cpp',build/'test.cpp');cmd=['g++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(build),str(build/'test.cpp')]+[str(build/n) for n in names if n.endswith('.cpp')]+['-o',str(build/'test')]
r=subprocess.run(cmd,capture_output=True,text=True);print(r.stdout+r.stderr,end='');r.check_returncode();r=subprocess.run([str(build/'test')]+(['baseline'] if a.baseline_zip else []),capture_output=True,text=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(r.stdout+r.stderr,end='');r.check_returncode();report=dict(version='0.8.42',baseline=bool(a.baseline_zip),physicalDeviceTested=False,sources=sources,results=json.loads(r.stdout),method='Real Workspace, Storage, Document and Encoding modules under AddressSanitizer/UBSan. Fault-injected filesystem distinguishes known absence and uncertain existence; LeakSanitizer disabled because this container cannot enumerate /proc threads; no leak or physical FAT claim.')
save_report(report,(release/'tools/verification'/('writer-storage-0842-baseline.json' if a.baseline_zip else 'writer-storage-0842.json')),here,r.stdout+r.stderr)
