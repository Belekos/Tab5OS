#!/usr/bin/env python3
"""Real map methods with host transport/FS boundaries; no physical timing claim."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.44_Ecriture';here=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--build',type=Path);ap.add_argument('--baseline',type=Path);a=ap.parse_args();build=a.build or Path(tempfile.mkdtemp(prefix='maps-0844-'));build.mkdir(parents=True,exist_ok=True)
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
paths=[sketch/x for x in ['Tab5Map.cpp','Tab5Map.h','Tab5AtomicFile.h','Tab5CooperativeClient.h']];hashes={str(p.relative_to(root)):sha(p) for p in paths}
text=(sketch/'Tab5Map.cpp').read_text();atomic=(sketch/'Tab5AtomicFile.h').read_text()
helperSigs=['static bool readFileCooperatively(', 'static bool writeFileCooperatively(', 'static bool closeWrittenMapFile(', 'static bool prepareMapTemporary(', 'static bool publishMapFile(', 'static bool ensureMapDirectory(']
helpers='\n'.join(extract(atomic,x) for x in ['inline bool tab5AtomicProbe(', 'inline bool tab5AtomicReplace('])+'\n'+'\n'.join(extract(text,x) for x in helperSigs)
functions='\n'.join(extract(text,x) for x in ['String Tab5MapEngine::cachePath(', 'bool Tab5MapEngine::ensureCacheDirectories(', 'bool Tab5MapEngine::storeTile('])
(build/'publication.cpp').write_text((here/'publication.cpp').read_text().replace('/* HELPERS */',helpers).replace('/* FUNCTIONS */',functions))
header=(sketch/'Tab5Map.h').read_text();header='\n'.join(x for x in header.splitlines() if not x.startswith('#include') and not x.startswith('#pragma')).replace(' private:',' public:')
lifeSigs=['void Tab5MapEngine::taskEntry(', 'void Tab5MapEngine::taskLoop(', 'void Tab5MapEngine::release(', 'void Tab5MapEngine::releaseSearch(']
def lifecycle(source,version):
 functions='\n'.join(extract(source,x) for x in lifeSigs)
 p=build/('lifecycle-'+version+'.cpp');p.write_text((here/'lifecycle.cpp').read_text().replace('/* HEADER */',header).replace('/* FUNCTIONS */',functions));return p
cases=[('publication',build/'publication.cpp'),('lifecycle',lifecycle(text,'0844'))]
if a.baseline:
 baselineText=(a.baseline/'Tab5Map.cpp').read_text();cases.append(('lifecycle-baseline',lifecycle(baselineText,'0843')))
 baselineFunctions='\n'.join(extract(baselineText,x) for x in ['String Tab5MapEngine::cachePath(', 'bool Tab5MapEngine::ensureCacheDirectories(', 'bool Tab5MapEngine::storeTile('])
 baselineHelpers='\n'.join(extract(baselineText,x) for x in ['static bool readFileCooperatively(', 'static bool writeFileCooperatively('])
 baselineMain=r'''int main(){Tab5MapEngine engine;const uint8_t bytes[]="replacement";Tab5MapTileResult tile;tile.data=bytes;tile.size=sizeof(bytes)-1;unsigned falseSuccesses=0;
 reset();fault.flush=EIO;if(engine.storeTile(tile))++falseSuccesses;CHECK(nodes[path]->bytes=="replacement");
 reset();fault.close=ETIMEDOUT;if(engine.storeTile(tile))++falseSuccesses;CHECK(nodes[path]->bytes=="replacement");
 reset();fault.write=ENOSPC;CHECK(!engine.storeTile(tile));CHECK(lastError==EIO);unchanged();
 CHECK(falseSuccesses==2);std::cout<<"checks="<<checks<<" PASS false_successes_on_sync_close="<<falseSuccesses<<" short_write_enospc_reported_eio=1\n";}
'''
 template=(here/'publication.cpp').read_text();template=template[:template.index('int main(){')]+baselineMain
 (build/'publication-baseline.cpp').write_text(template.replace('/* HELPERS */',baselineHelpers).replace('/* FUNCTIONS */',baselineFunctions));cases.append(('publication-baseline',build/'publication-baseline.cpp'))
proof=root/'tools/verification/maps-0844';proof.mkdir(parents=True,exist_ok=True);result={};env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
for name,source in cases:
 cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sketch),str(source),'-lpthread','-o',str(build/name)]
 p=subprocess.run(cmd,capture_output=True,text=True);(proof/(name+'-build.log')).write_text(p.stdout+p.stderr)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 p=subprocess.run([str(build/name)],capture_output=True,text=True,env=env,timeout=60);output=p.stdout+p.stderr;(proof/(name+'-run.log')).write_text(output);print(name,output)
 if p.returncode:raise RuntimeError(name+' failed')
 result[name]={'checks':int(re.search(r'checks=(\d+) PASS',output)[1]),'exitCode':p.returncode,'output':output}
 if name=='lifecycle':assert 'cancelled_throttle_ms=0 radio_cancelled_throttle_ms=0 stale_generation_throttle_ms=0 media_yield_throttle_ms=0' in output
 if name=='lifecycle-baseline':assert 'cancelled_throttle_ms=60 radio_cancelled_throttle_ms=220 stale_generation_throttle_ms=60 media_yield_throttle_ms=60' in output
stable=hashes=={str(p.relative_to(root)):sha(p) for p in paths};assert stable
report={'version':'0.8.44','passed':True,'sourceStableDuringTests':stable,'sourceHashes':hashes,'physicalDeviceTested':False,'baselineHashes':({str(a.baseline/'Tab5Map.cpp'):sha(a.baseline/'Tab5Map.cpp')} if a.baseline else {}),'results':result,'testSourceHashes':{p.relative_to(root).as_posix():sha(p) for p in here.glob('*') if p.is_file()},'checks':sum(x['checks'] for x in result.values()),'limits':['Production map file publication helpers/storeTile/cachePath/ensureCacheDirectories and worker lifecycle extracted verbatim. Host filesystem, socket and FreeRTOS boundaries simulated.','Lifecycle milliseconds count intentional vTaskDelay throttle values, not measured wall time or physical task deletion timing.','No physical SD, Bluetooth, Wi-Fi, radio, display, CPU or heap measurement. LeakSanitizer unavailable under ptrace; AddressSanitizer/UBSan active, capability payload and file handles explicitly tracked.']}
(root/'tools/verification/maps-0844.json').write_text(json.dumps(report,indent=2)+'\n')
