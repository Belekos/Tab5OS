#!/usr/bin/env python3
"""Production Meshtastic protobuf + callbacks/UI; ESP/NimBLE transport is simulated."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--lvgl',required=True,type=Path);p.add_argument('--lvgl-build',required=True,type=Path);p.add_argument('--build',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;s=r/'Tab5OS_Arduino_v0.8.41_Ecriture';w=a.build.resolve();w.mkdir(parents=True,exist_ok=True);e=r/'tools/verification/meshtastic-0841';e.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
productionPaths=[s/n for n in ['Tab5Meshtastic.cpp','Tab5Meshtastic.h','Tab5MeshtasticProtocol.h','Tab5MeshModel.h','Tab5MeshMemory.h','Tab5MeshAllocator.c','Tab5MeshAllocator.h','Tab5MeshAllocatorEspIdf554.inc','Tab5ResourceManager.cpp','Tab5ResourceManager.h','Tab5ResourcePolicy.h','Tab5MeshCodec.h','Tab5MeshStore.cpp','Tab5MeshStore.h','Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5UiStyle.cpp','Tab5UiStyle.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','sdcard/System/font.ttf']]
integrationPaths=[]
testPaths=sorted(p for p in here.rglob('*') if p.is_file())
paths=productionPaths+integrationPaths+testPaths
original={str(p.relative_to(r)):sha(p) for p in paths}
text=(s/'Tab5Meshtastic.cpp').read_text();at=text.index('namespace Tab5Meshtastic {');(w/'production-meshtastic.inc').write_text(text[at:])
store=(s/'Tab5MeshStore.cpp').read_text();(w/'production-store.inc').write_text(store[store.index('namespace Tab5MeshStore {'):])
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(s),'-iquote',str(w),'-I'+str(here),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include')]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
results={}
# Compile the actual SDK allocator replacement in C, preserving linkage/caps.
allocator=w/'allocator.o'
cflags=['-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(here/'allocator-stubs'),'-I'+str(s)]
for suffix,extra in [('',[]),('-fallback',['-DESP_IDF_VERSION=0'])]:
 obj=w/('allocator'+suffix+'.o')
 cmd=['gcc',*cflags,*extra,'-c',str(s/'Tab5MeshAllocator.c'),'-o',str(obj)]
 result=subprocess.run(cmd,capture_output=True,text=True)
 (e/('allocator'+suffix+'-build.log')).write_text(result.stdout+result.stderr)
 if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
 symbols=subprocess.check_output(['nm','-g','--defined-only',str(obj)],text=True)
 (e/('allocator'+suffix+'-symbols.log')).write_text(symbols)
 exports={line.split()[-1] for line in symbols.splitlines() if 'bt_osi_mem_' in line}
 expected={'bt_osi_mem_'+fn for fn in ['malloc','calloc','free','malloc_internal','calloc_internal','free_internal']}
 assert exports==(set() if suffix else expected),exports
results['allocator-fallback']={'passed':True,'unknownSdkExports':0,'reviewedSdkExports':6}
for name,src,extra,args in [('memory','memory.cpp',[str(allocator),str(s/'Tab5ResourceManager.cpp'),'-I'+str(here/'allocator-stubs'),'-pthread'],[]),('protocol','protocol.cpp',[],[]),('codec-store','codec-store.cpp',[],[]),('backend-ui','backend-ui.cpp',[*[str(s/n) for n in ['Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','WriterDocument.cpp']],str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread'],[str(e),str(s/'sdcard/System/font.ttf')])]:
 # Keep executables out of header lookup names: 'memory' shadows <memory>.
 executable=w/('test-'+name)
 cmd=['g++',*flags,str(here/src),*extra,'-o',str(executable)];b=subprocess.run(cmd,capture_output=True,text=True);(w/(name+'-build.log')).write_text(b.stdout+b.stderr);(e/(name+'-build.log')).write_text(b.stdout+b.stderr)
 if b.returncode:print(b.stdout+b.stderr);raise SystemExit(b.returncode)
 t=subprocess.run([str(executable),*args],capture_output=True,text=True,env=env,timeout=120);print(t.stdout+t.stderr);(e/(name+'-run.log')).write_text(t.stdout+t.stderr)
 if t.returncode:raise SystemExit(t.returncode)
 results[name]=json.loads(t.stdout.strip().splitlines()[-1])
assert original=={str(p.relative_to(r)):sha(p) for p in paths},'production changed during tests'
from PIL import Image
for ppm in e.glob('*.ppm'):Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
integration={}
report=dict(integrationSourceChecks=integration,integrationSourceHashes={str(p.relative_to(r)):original[str(p.relative_to(r))] for p in integrationPaths},version='0.8.41',passed=True,physicalDeviceTested=False,method=__doc__,sourceHashes={str(p.relative_to(r)):original[str(p.relative_to(r))] for p in productionPaths},testSourceHashes={str(p.relative_to(r)):original[str(p.relative_to(r))] for p in testPaths},evidenceHashes={str(p.relative_to(r)):sha(p) for p in sorted(e.iterdir()) if p.is_file()},sanitizers=['address','undefined'],results=results,lvglArchiveSha256=sha(a.lvgl_build/'liblvgl.a'),limitations=['ESP/NimBLE entry points simulated; no physical C6 firmware, over-the-air pairing/MTU or radio interoperability measured.','NimBLE worker scheduler, successful host startup and physical shutdown are not exercised; callback bodies, retry policy, dual-heap rejection and startup error cleanup are. Target compilation uses actual SDK headers.', 'Allocator tests compile the unchanged copied SDK allocator and simulate heap caps/failure. The 200 pool rollback cycles exercise caller-owned allocations, not the entire SDK startup graph. SDK fatal malloc assertions are preserved; actual board peak/fragmentation must be measured.','LeakSanitizer unavailable in the executor; application allocation/free counters cover UI sessions and persistence jobs.'])
(r/'tools/verification/meshtastic-0841.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
