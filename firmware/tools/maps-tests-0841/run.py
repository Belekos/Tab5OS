#!/usr/bin/env python3
"""Execute production gazetteer and extracted map request/worker/marker methods.
File, FreeRTOS, network and Mesh boundaries are simulated; actual SD index is read.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.41_Ecriture';here=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--build',type=Path);ap.add_argument('--final',action='store_true');a=ap.parse_args();build=a.build or Path(tempfile.mkdtemp(prefix='maps-0841-'));build.mkdir(parents=True,exist_ok=True)
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
paths=[sketch/n for n in ['Tab5Map.cpp','Tab5Map.h','Tab5GeoIndex.h','Tab5GeoFold.h','Tab5MapViewPolicy.h','Tab5MapViewState.h','Tab5MapWheel.h']]+[root/'tools/maps-gazetteer/build.py']+list((sketch/'sdcard/System/maps/gazetteer').glob('*'))
source_hashes={p.relative_to(root).as_posix():sha(p) for p in paths}
text=(sketch/'Tab5Map.cpp').read_text();header=(sketch/'Tab5Map.h').read_text();header='\n'.join(x for x in header.splitlines() if not x.startswith('#include') and not x.startswith('#pragma')).replace(' private:',' public:')
signatures=['bool Tab5MapEngine::createQueues(', 'void Tab5MapEngine::discardSearchRequests(', 'bool Tab5MapEngine::requestSearch(', 'bool Tab5MapEngine::pollSearch(', 'void Tab5MapEngine::releaseSearch(', 'void Tab5MapEngine::release(', 'void Tab5MapEngine::taskEntry(', 'void Tab5MapEngine::taskLoop(', 'bool Tab5MapEngine::localSearch(', 'size_t Tab5MapEngine::markerCount(', 'bool Tab5MapEngine::marker(', 'uint32_t Tab5MapEngine::markerRevision(', 'void Tab5MapEngine::selectSearchMarker(']
functions=extract(text,'class GazetteerReader {')+';\n'+'\n'.join(extract(text,s) for s in signatures)
(build/'worker.cpp').write_text((here/'worker.cpp').read_text().replace('/* HEADER */',header).replace('/* FUNCTIONS */',functions))
proof=root/'tools/verification/maps-0841';proof.mkdir(exist_ok=True,parents=True);results={};env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
for name,source in [('core',here/'core.cpp'),('worker',build/'worker.cpp'),('wheel',here/'wheel.cpp')]:
 cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sketch),str(source),'-lpthread','-o',str(build/name)]
 p=subprocess.run(cmd,capture_output=True,text=True);(proof/(name+'-build.log')).write_text(p.stdout+p.stderr)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 p=subprocess.run([str(build/name),str(sketch/'sdcard/System/maps/gazetteer/cities.t5geo')],capture_output=True,text=True,env=env,timeout=120);output=p.stdout+p.stderr;(proof/(name+'-run.log')).write_text(output);print(output)
 if p.returncode:raise RuntimeError(name+' failed')
 results[name]={'checks':int(re.search(r'checks=(\d+) PASS',output)[1]),'exitCode':p.returncode}
stable=all(sha(root/p)==h for p,h in source_hashes.items());assert stable
report=dict(version='0.8.41',passed=a.final,draft=not a.final,checks=sum(x['checks'] for x in results.values()),sourceStableDuringTests=stable,physicalDeviceTested=False,sourceHashes=source_hashes,testSourceHashes={p.relative_to(root).as_posix():sha(p) for p in here.glob('*') if p.is_file()},evidenceHashes={p.relative_to(root).as_posix():sha(p) for p in proof.iterdir() if p.is_file()},result=results,method=__doc__,limits=['Map search requests/taskLoop/taskEntry/reader/marker methods extracted verbatim from production. File/queues/allocations/network/Mesh API are host adapters; no physical SD or radio timing measured.','Host ASan/UBSan; LeakSanitizer disabled. Tracked capability allocations return to zero after cancellation/OOM/session loops.','GeoNames cities1000 covers named populated places and administrative seats, not all streets or POIs.'],database=json.loads((sketch/'sdcard/System/maps/gazetteer/cities.json').read_text()))
(root/'tools/verification/maps-0841.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n');print(json.dumps({'passed':a.final,'stable':stable,'checks':report['checks']}))
