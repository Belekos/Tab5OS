#!/usr/bin/env python3
"""Production C core with independent flood/signed-feather oracles, OOM and verified .40 inversion reproduction."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);a=p.parse_args()
tests=Path(__file__).resolve().parent;root=tests.parents[1];core=root/'Tab5OS_Arduino_v0.8.41_Ecriture/tools/native-apps/image-editor';a.build.mkdir(parents=True,exist_ok=True)
proof=root/'tools/verification/drawing-core-0841';proof.mkdir(parents=True,exist_ok=True)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def hashes(paths):return {str(x.relative_to(root)):digest(x) for x in paths}
source=hashes([core/'image_core.c',core/'image_core.h',core/'image_selection.h'])
test=hashes([tests/'core.c',Path(__file__).resolve(),core/'test_core.c',tests/'baseline-inversion.c',tests/'baseline-image-selection-0840.h'])
flags=['-std=c11','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def run(cmd,name):
 r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120)
 text=r.stdout+r.stderr;(a.build/name).write_text(text);(proof/name).write_text(text)
 if r.returncode:print(text);raise SystemExit(r.returncode)
 return r.stdout
run(['cc',*flags,'-I',str(core),str(tests/'core.c'),str(core/'image_core.c'),'-o',str(a.build/'core')],'compile-core.log')
result=run([str(a.build/'core')],'core.log');print(result)
# Compile the unchanged .40 selection implementation, checking provenance
# against its historical report. The core C file is unchanged; public .41
# header differences are documentation only, with identical ABI.
old=json.loads((root/'tools/verification/drawing-core-0840.json').read_text())['sourceHashes']
oldprefix='Tab5OS_Arduino_v0.8.40_Ecriture/tools/native-apps/image-editor/'
fixture=tests/'baseline-image-selection-0840.h'
assert digest(fixture)==old[oldprefix+'image_selection.h']
assert digest(core/'image_core.c')==old[oldprefix+'image_core.c']
baseline=a.build/'baseline-0840';baseline.mkdir(exist_ok=True)
for name in ('image_core.c','image_core.h'):(baseline/name).write_bytes((core/name).read_bytes())
(baseline/'image_selection.h').write_bytes(fixture.read_bytes())
before_after={}
for name,src,args in [('baseline',baseline,[]),('fixed',core,['--fixed'])]:
 exe=a.build/('inversion-'+name)
 run(['cc',*flags,'-I',str(src),str(tests/'baseline-inversion.c'),str(src/'image_core.c'),'-o',str(exe)],'compile-inversion-'+name+'.log')
 before_after[name]=run([str(exe),*args],'inversion-'+name+'.log').strip();print(name+': '+before_after[name])
for path,value in dict(source,**test).items():assert digest(root/path)==value
report={'version':'0.8.41','passed':True,'physicalDeviceTested':False,'method':__doc__,'sourceHashes':source,'testSourceHashes':test,'flags':flags,'result':json.loads(result.strip().splitlines()[-1]),'baselineRegression':{'verifiedSelection0840Sha256':digest(fixture),'baselineExpectedBug':before_after['baseline'],'fixedExpectedSelection':before_after['fixed']},'evidenceHashes':hashes(sorted(proof.glob('*.log'))),'limitations':['Host CPU only; no tablet timings or physical mouse/touch test.','LSan cannot inspect process threads in this container; custom allocator verifies zero outstanding bytes at every scenario teardown.','Memory failure may evict old history under the existing document snapshot policy; current pixels and selection must remain intact.']}
(proof.parent/'drawing-core-0841.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
