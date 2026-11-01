#!/usr/bin/env python3
"""Run selection ownership tests against the real C editor and its 0.8.49 witness."""
import argparse,hashlib,json,os,subprocess,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',required=True,type=Path);p.add_argument('--baseline',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.51_Ecriture';core=sketch/'tools/native-apps/image-editor';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True)
inputs=[core/x for x in ['image_core.c','image_core.h','image_selection.h','test_core.c']]+[here/x for x in ['test.c','regression.c','run.py']]
def hashes():return {str(x.relative_to(release)):hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}
before=hashes();env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
flags=['-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror']
def run(cmd):
 r=subprocess.run([str(x)for x in cmd],capture_output=True,text=True,env=env)
 if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
 return r.stdout
results={}
for name,source in [('new',here/'test.c'),('legacy',core/'test_core.c')]:
 run(['cc',*flags,'-I'+str(core),source,core/'image_core.c','-o',work/name]);results[name]=json.loads(run([work/name]))
baseline=work/'baseline';baseline.mkdir(exist_ok=True)
archive_sha=hashlib.sha256(a.baseline.read_bytes()).hexdigest();assert archive_sha=='0a6928c6f494f076d359d1ede44a6609a3bf9978e7970b604469787a5c0ee861'
with zipfile.ZipFile(a.baseline)as z:
 prefix='Tab5OS_Ecriture_v0.8.49/Tab5OS_Arduino_v0.8.49_Ecriture/tools/native-apps/image-editor/'
 for name in ['image_core.c','image_core.h','image_selection.h']:(baseline/name).write_bytes(z.read(prefix+name))
for name,path in [('before',baseline),('after',core)]:
 run(['cc',*flags,'-I'+str(path),here/'regression.c',path/'image_core.c','-o',work/name]);results[name]=json.loads(run([work/name]))
assert results['before']=={'destinationRestored':False,'fractionalRoundTripExact':False},results['before']
assert results['after']=={'destinationRestored':True,'fractionalRoundTripExact':True},results['after']
assert before==hashes(),'Sources changed during validation; rerun on frozen source'
proof=dict(results['new'],version='0.8.51',legacy=results['legacy'],baseline={'before':results['before'],'after':results['after'],'archiveSha256':archive_sha},sourceHashes=before,physicalDeviceTested=False,sanitizers=['ASan','UBSan'],limits=['Real C model; no physical PSRAM/LVGL latency claim.','Shared floating content is charged to the bounded document allocator; history byte cap counts raster snapshots.','LeakSanitizer unavailable; every test asserts zero tracked allocations at destruction.'])
output=release/'tools/verification/image-selection-0851.json';output.parent.mkdir(exist_ok=True);output.write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof,indent=2))
