#!/usr/bin/env python3
"""Selection/fill production core, independent flood/feather oracles and allocation faults."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);a=p.parse_args()
tests=Path(__file__).resolve().parent;root=tests.parents[1];core=root/'Tab5OS_Arduino_v0.8.40_Ecriture/tools/native-apps/image-editor';a.build.mkdir(parents=True,exist_ok=True)
def hashes(paths):return {str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths}
source=hashes([core/'image_core.c',core/'image_core.h',core/'image_selection.h']);test=hashes([tests/'core.c',Path(__file__).resolve(),core/'test_core.c'])
flags=['-std=c11','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
cmd=['cc',*flags,'-I',str(core),str(tests/'core.c'),str(core/'image_core.c'),'-o',str(a.build/'core')]
r=subprocess.run(cmd,capture_output=True,text=True);(a.build/'compile-core.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(a.build/'core')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=120)
(a.build/'core.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
for path,digest in dict(source,**test).items():assert hashlib.sha256((root/path).read_bytes()).hexdigest()==digest
report={'version':'0.8.40','passed':True,'physicalDeviceTested':False,'method':__doc__,'sourceHashes':source,'testSourceHashes':test,'flags':flags,'result':json.loads(r.stdout.strip().splitlines()[-1]),'evidenceHashes':{},'limitations':['Host CPU only; no tablet timings or physical mouse/touch test.','LSan cannot inspect process threads in this container; custom allocator verifies zero outstanding bytes at every scenario teardown.','Memory failure may evict old history under the existing document snapshot policy; current pixels and selection must remain intact.']}
proof=root/'tools/verification';proof.mkdir(exist_ok=True);(proof/'drawing-core-0840.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
