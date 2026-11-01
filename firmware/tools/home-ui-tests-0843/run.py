#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,os,re,subprocess
r=Path(__file__).resolve().parents[2];s=r/'Tab5OS_Arduino_v0.8.43_Ecriture';t=Path(__file__).resolve().parent;o=r/'tools/verification/home-ui-0843';o.mkdir(exist_ok=True)
build=Path('/workspace/scratch/644cc83c43c8/build_0842/home-ui-0843');build.mkdir(exist_ok=True)
lvgl=Path('/workspace/scratch/644cc83c43c8/build_0842/host-dependencies/lvgl-9.6.0');lib=Path('/workspace/scratch/644cc83c43c8/host_lvgl_0842')
old=t/'baseline'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
result={}
definitionHashes={}
sourceFiles=[s/n for n in ['Tab5HomeUi.h','Tab5AppRefresh.h','Tab5AppCatalogue.cpp','Tab5AppCatalogue.h','Tab5AppCataloguePolicy.h','Tab5ShellLayout.h','Tab5ShellPolicy.h','Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','Tab5Task.h','Tab5ResourceManager.h','Tab5ResourcePolicy.h','WriterEncoding.h','Tab5Serial.h','Tab5SerialCore.h','Tab5Ico.h','Tab5PythonServices.h','Tab5Storage.h','Tab5VolumeFS.h','Tab5UsbStorage.h']]
initialHashes={str(p.relative_to(r)):sha(p) for p in sourceFiles}
testFiles=[p for p in t.rglob('*') if p.is_file() and '__pycache__' not in str(p)]
initialTestHashes={str(p.relative_to(r)):sha(p) for p in testFiles}
for name,src,home,base in [('before',old,t/'baseline',1),('after',s,s,0)]:
 w=build/name;w.mkdir(exist_ok=True)
 (w/'production-catalogue.inc').write_text('#include "'+str(src/'Tab5AppCatalogue.cpp')+'"\n')
 (w/'production-home.inc').write_text('#include "'+str(home/'Tab5HomeUi.h')+'"\n')
 if base: definitions=(t/'baseline/home-defs.inc').read_text()
 else:
  code=(src/(src.name+'.ino')).read_text();types=(src/'Tab5OSTypes.h').read_text()
  defs=[re.search(r'enum class HomeAction[^;]+;',types,re.S)[0],re.search(r'struct HomeCardDef\s*{.*?};',types,re.S)[0]]
  for n in ['HOME_CARDS','HOME_IDS','HOME_SYMBOLS']:defs.append(re.search(r'static const [^\n]*\b'+n+r'\[\][^;]+;',code,re.S)[0])
  definitions='\n'.join(defs)
 (w/'production-home-defs.inc').write_text(definitions)
 definitionHashes[name]=hashlib.sha256(definitions.encode()).hexdigest()
 flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-DLV_CONF_PATH="'+str(lib/'lv_conf.h')+'"','-DTEST_BASELINE='+str(base),'-I'+str(t/'mocks'),'-I'+str(t),'-I'+str(w),'-I'+str(src),'-I'+str(lvgl),'-I'+str(lvgl/'src'),'-Wno-cpp','-Wno-deprecated-declarations']
 cmd=['g++',*flags,str(t/'home.cpp'),*[str(src/n) for n in ['Tab5Focus.cpp','Tab5TextInput.cpp','WriterDocument.cpp']],str(lib/'liblvgl.a'),'-pthread','-o',str(w/'home')]
 cp=subprocess.run(cmd,capture_output=True,text=True);(o/(name+'-build.log')).write_text(cp.stdout+cp.stderr)
 if cp.returncode:print(cp.stderr[-10000:]);raise SystemExit(cp.returncode)
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 cp=subprocess.run([str(w/'home')],capture_output=True,text=True,env=env);(o/(name+'-run.log')).write_text(cp.stdout+cp.stderr)
 if cp.returncode:print(cp.stdout+cp.stderr);raise SystemExit(cp.returncode)
 result[name]=json.loads(cp.stdout.strip().splitlines()[-1]);print(name,result[name])
assert initialHashes=={str(p.relative_to(r)):sha(p) for p in sourceFiles},'Source changed during tests'
assert initialTestHashes=={str(p.relative_to(r)):sha(p) for p in testFiles},'Test changed during run'
report=dict(version='0.8.43',passed=True,physicalDeviceTested=False,sourceStableDuringTests=True,simulatedLoopMilliseconds=8,lvglArchiveSha256=sha(lib/'liblvgl.a'),nativeDefinitionsSha256=definitionHashes,results=result,sourceHashes=initialHashes,testSourceHashes=initialTestHashes,evidenceHashes={str(p.relative_to(r)):sha(p) for p in o.iterdir() if p.is_file()},method='Entire actual HomeUi construction/render/events/service and Catalogue linked to real LVGL9.6, actual Focus/TextInput/WriterDocument. Filesystem/Preferences/Python manifest/tools/palette/clock are boundary adapters. Native definitions extracted from actual main.',limitations=['No physical device; counts are synchronous filesystem calls, not measured SD latency or P4 CPU.','Native app launch callbacks do not execute hardware apps. Translation T is identity (French source keys); this test is not a14-language layout test. Builtin calculator/Python icon helpers use glyph placeholders; card construction and labels/render/invalidation are actual.','LeakSanitizer disabled under ptrace; ASan/UBSan active.','The LVGL loop contains only tested Home and Focus timers, not complete OS background services.','Async worker integration uses actual std::thread with explicit joins in test-only boundaries; it validates publication/cancellation, not scheduling latency.'])
(o.parent/'home-ui-0843.json').write_text(json.dumps(report,indent=2)+'\n')
