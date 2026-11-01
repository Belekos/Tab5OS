"""Real LVGL/ArduinoJson host regression tests, without hardware or network.

Usage: python3 tools/maps-tests-0832/run.py --libraries /path/to/Arduino/libraries
GCC/G++, ar and Python3 are required. All build files stay outside the release.
"""
from pathlib import Path
import argparse,concurrent.futures,datetime,hashlib,json,os,subprocess,sys,tempfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--libraries',required=True,type=Path)
parser.add_argument('--build',type=Path)
parser.add_argument('--jobs',type=int,default=4)
args=parser.parse_args()
here=Path(__file__).resolve().parent;release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.32_Ecriture'
work=(args.build or Path(tempfile.mkdtemp(prefix='tab5-maps-0832-'))).resolve();work.mkdir(parents=True,exist_ok=True)
lvgl=args.libraries.resolve()/'lvgl';arduinojson=args.libraries.resolve()/'ArduinoJson/src'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def extract(text,signature):
 start=text.index(signature);i=text.index('{',start);depth=0;state='code'
 while i<len(text):
  c=text[i]
  if state=='line':
   if c in '\r\n':state='code'
  elif state=='block':
   if text[i:i+2]=='*/':state='code';i+=1
  elif state in ('"',"'"):
   if c=='\\':i+=1
   elif c==state:state='code'
  elif text[i:i+2]=='//':state='line';i+=1
  elif text[i:i+2]=='/*':state='block';i+=1
  elif c in ('"',"'"):state=c
  elif c=='{':depth+=1
  elif c=='}':
   depth-=1
   if not depth:return text[start:i+1]
  i+=1
 raise ValueError(signature)
for p in [lvgl/'library.properties',arduinojson.parent/'library.properties']:
 props=p.read_text();expected='9.6.0' if p.parent==lvgl else '7.4.3'
 if 'version='+expected not in props:raise RuntimeError('Wrong pinned dependency: '+str(p))
config=(sketch/'lv_conf.h').read_text().replace('#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM','#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB')
(work/'lv_conf.h').write_text(config)
flags=['-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(work/'lv_conf.h')+'"']
flags+=['-I'+str(p) for p in [lvgl,lvgl/'include',arduinojson,sketch]]
# Apply exactly the release's existing allocation-safe Arabic shaping patch
# to the single host object; the installed dependency is never changed.
sys.path.insert(0,str(release/'tools/build_clang'))
from prepare_lvgl import patch_shaper
original=(lvgl/'src/misc/lv_text_ap.c').read_bytes();patched,_,_=patch_shaper(original)
(work/'lv_text_ap.c').write_bytes(patched)
# This source uses relative headers: compile via -iquote to its original folder.
objects=work/'objects';objects.mkdir(exist_ok=True)
sources=[p for p in sorted((lvgl/'src').rglob('*')) if p.suffix in ('.c','.cpp','.S') and 'thorvg' not in str(p) and 'vg_lite_tvg' not in str(p)]
dependency_sources={p.relative_to(lvgl).as_posix():sha(p) for p in sources}
def compile_lv(p):
 object_path=objects/('_'.join(p.relative_to(lvgl).parts)+'.o')
 source=work/'lv_text_ap.c' if p.name=='lv_text_ap.c' else p
 signature=hashlib.sha256((sha(source)+'\n'+repr(flags)).encode()).hexdigest();stamp=object_path.with_suffix('.sha256')
 if object_path.exists() and stamp.exists() and stamp.read_text()==signature:return object_path
 command=['g++' if p.suffix=='.cpp' else 'gcc',*flags,'-iquote',str(p.parent),'-c',str(source),'-o',str(object_path)]
 result=subprocess.run(command,capture_output=True,text=True)
 if result.returncode:raise RuntimeError(str(p)+'\n'+result.stdout+result.stderr)
 stamp.write_text(signature);return object_path
with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,args.jobs)) as pool:
 compiled=list(pool.map(compile_lv,sources))
subprocess.run(['ar','rcs',str(work/'liblvgl.a'),*map(str,compiled)],check=True)
text=(sketch/(sketch.name+'.ino')).read_text();item=extract((sketch/'Tab5Map.h').read_text(),'struct Tab5MapSearchItem')+';'
names=['static lv_obj_t* makeButton(', 'static lv_obj_t* audioRowTitle(', 'static void stopAudioRowMarquee(', 'static lv_obj_t* createMapOverlay(', 'static void closeMapSearchEvent(', 'static void mapSearchResultEvent(', 'static void showMapSearchResults() {']
controllers={sig:extract(text,sig) for sig in names}
(work/'ui.cpp').write_text((here/'ui.cpp').read_text().replace('/* ITEM */',item).replace('/* FUNCTIONS */','\n'.join(controllers.values())))
numeric=extract((sketch/'Tab5NumericKeyboard.h').read_text(),'inline unsigned mode(')
for name,source in [('ime-fixed',sketch/'Tab5ImeUi.inc'),('ime-baseline',here/'Tab5ImeUi-0830.inc.txt')]:
 body=(here/'ime.cpp').read_text().replace('/* BUTTON */',controllers['static lv_obj_t* makeButton(']).replace('/* NUMERIC */',numeric).replace('/* IME */',source.read_text())
 (work/(name+'.cpp')).write_text(body)

encoder=extract((sketch/'Tab5Map.cpp').read_text(),'static String urlEncoded(')
(work/'parser.cpp').write_text((here/'parser.cpp').read_text().replace('/* ITEM */',item).replace('/* URL_ENCODER */',encoder))
engine=(sketch/'Tab5Map.cpp').read_text();header=(sketch/'Tab5Map.h').read_text()
workerFunctions={sig:extract(engine,sig) for sig in ['bool Tab5MapEngine::requestSearch(', 'bool Tab5MapEngine::pollSearch(', 'void Tab5MapEngine::releaseSearch(', 'void Tab5MapEngine::taskLoop(', 'void Tab5MapEngine::taskEntry(']}
workerBody=(here/'worker.cpp').read_text().replace('/* ITEM */',item).replace('/* FUNCTIONS */','\n'.join(workerFunctions.values()))
for marker,signature in [('SEARCH_RESULT','struct Tab5MapSearchResult'),('TILE_RESULT','struct Tab5MapTileResult'),('SEARCH_REQUEST','struct SearchRequest'),('TILE_REQUEST','struct TileRequest')]:
 workerBody=workerBody.replace('/* '+marker+' */',extract(header,signature)+';')
(work/'worker.cpp').write_text(workerBody)

def compile_program(name,paths):
 command=['g++',*flags,*map(str,paths),'-lpthread',*(['-Wl,--wrap=lv_obj_add_event_cb'] if name.startswith('ime-') else []),'-o',str(work/name)]
 r=subprocess.run(command,capture_output=True,text=True)
 (work/(name+'-compile.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stdout+r.stderr)
compile_program('parser',[work/'parser.cpp'])
compile_program('worker',[work/'worker.cpp'])
for name,focus in [('ui-fixed',sketch/'Tab5Focus.cpp')]:
 compile_program(name,[work/'ui.cpp',focus,sketch/'Tab5TextInput.cpp',sketch/'WriterDocument.cpp',work/'liblvgl.a'])
for name in ['ime-fixed','ime-baseline']:
 compile_program(name,[work/(name+'.cpp'),sketch/'Tab5Focus.cpp',sketch/'Tab5TextInput.cpp',sketch/'WriterDocument.cpp',work/'liblvgl.a'])
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
results={}
def run(name,argv,baseline=False):
 # Reused workspace artifacts can retain a non-executable file mode.
 # Only launch binaries just compiled into this dedicated host directory.
 executable=Path(argv[0]);assert executable.parent==work
 executable.chmod(executable.stat().st_mode | 0o111)
 r=subprocess.run(list(map(str,argv)),capture_output=True,text=True,env=env,timeout=120)
 output=r.stdout+r.stderr;(work/(name+'.log')).write_text(output)
 if baseline:(here/('baseline-'+name+'-asan.log')).write_text(output)
 results[name]={'exitCode':r.returncode,'output':output if not baseline else output.split('Shadow bytes')[0], 'heapUseAfterFreeDetected':'heap-use-after-free' in output}
 print(name,r.returncode,r.stdout.strip(),flush=True)
run('parser',[work/'parser'])
run('worker',[work/'worker'])
for mode in ['enter','space','submit','focus-initial']:
 argv=[str(sketch/'sdcard/System/font.ttf')]+([] if mode=='enter' else [mode])
 run('fixed-'+mode,[work/'ui-fixed',*argv])

run('fixed-ime',[work/'ime-fixed',sketch/'sdcard/System/font.ttf'])
run('old-ime',[work/'ime-baseline',sketch/'sdcard/System/font.ttf'],baseline=True)
passed=all(results['fixed-'+m]['exitCode']==0 for m in ['enter','space','submit','focus-initial','ime']) and results['parser']['exitCode']==0 and results['worker']['exitCode']==0 and results['old-ime']['exitCode']!=0 and results['old-ime']['heapUseAfterFreeDetected']
paths=[sketch/p for p in ['Tab5ImeUi.inc','Tab5Ime.h','Tab5NumericKeyboard.h','Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','WriterDocument.cpp','WriterDocument.h','Tab5Map.cpp','Tab5Map.h','Tab5MapSearchParser.h','lv_conf.h',sketch.name+'.ino','sdcard/System/font.ttf']]
paths += [p for p in here.iterdir() if p.is_file()]
paths += [release/'tools/build_clang/prepare_lvgl.py',release/'tools/build_clang/prepare_m5gfx.py']
report={'version':'0.8.32','passed':passed,'physicalDeviceTested':False,'hardwareFailureReproduced':False,
 'method':'Exact map result controllers and URL encoder extracted from production; production Focus/TextInput/WriterDocument with LVGL9.6.0 and delivered TinyTTF font, production parser with ArduinoJson7.4.3 and the complete actual IME include with small app/storage stubs, actual composer/numeric-input logic. GCC host ASan+UBSan, stock OS allocations in place of PSRAM. Existing release Arabic shaping patch applied. Actual Maps taskLoop/taskEntry/request/poll/release run over host mutex/queue/clock adapters and scripted transfer outcomes; no HTTP/TLS/C6/SD hardware simulated, no real provider query.',
 'confirmedDefects':['The IME service retained a freed search textarea, including for a Latin keyboard: imePhysical -> imeSync -> submit/delete -> imeService dereferenced it.'],
 'limitation':'The .30 dangling IME pointer is reproduced on the actual search input lifetime with real LVGL, including Latin mode and successive reopen. No physical device, actual network request or TLS handshake has been tested; exact correspondence to the user tablet crash is not established.',
 'results':results,'sourceHashes':{p.relative_to(release).as_posix():sha(p) for p in paths},
 'extractedControllersSha256':{k:hashlib.sha256(v.encode()).hexdigest() for k,v in controllers.items()},
 'encoderSha256':hashlib.sha256(encoder.encode()).hexdigest(),
 'extractedWorkerSha256':{k:hashlib.sha256(v.encode()).hexdigest() for k,v in workerFunctions.items()},
 'dependencyEvidence':{'lvglVersion':'9.6.0','arduinoJsonVersion':'7.4.3','lvglCompiledSourceCount':len(sources),'sourceManifestSha256':hashlib.sha256(json.dumps(dependency_sources,sort_keys=True).encode()).hexdigest(),'lvglTextApiSha256':sha(lvgl/'src/core/lv_obj.c'),'lvglCompatApiSha256':sha(lvgl/'include/lvgl/api_map/lv_api_map_v9_5.h'),'arabicPatchedSha256':hashlib.sha256(patched).hexdigest(),'arduinoJsonHeaderSha256':sha(arduinojson/'ArduinoJson.h'),'hostConfigSha256':sha(work/'lv_conf.h')},
 'toolchain':subprocess.check_output(['g++','--version'],text=True).splitlines()[0],'timestampUtc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
(release/'tools/verification/maps-0832.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print('PASS' if passed else 'FAIL',str(release/'tools/verification/maps-0832.json'))
raise SystemExit(0 if passed else 1)
