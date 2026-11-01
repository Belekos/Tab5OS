#!/usr/bin/env python3
"""Actual map worker/requests/generation/reader methods with threaded queue and HTTP/FS boundaries."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path);p.add_argument('--build',type=Path);a=p.parse_args();here=Path(__file__).resolve().parent;r=here.parent.parent;s=next(r.glob('Tab5OS_Arduino_v*_Ecriture'));b=(a.build or r.parent.parent/'build_0845/maps-worker').resolve();b.mkdir(exist_ok=True,parents=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def extract(text,signature):
 begin=text.index(signature);i=text.index('{',begin);depth=0;state='code'
 while i<len(text):
  c=text[i]
  if state=='line':
   if c in '\r\n':state='code'
  elif state=='comment':
   if text[i:i+2]=='*/':state='code';i+=1
  elif state in ('"',"'"):
   if c=='\\':i+=1
   elif c==state:state='code'
  elif text[i:i+2]=='//':state='line';i+=1
  elif text[i:i+2]=='/*':state='comment';i+=1
  elif c in ('"',"'"):state=c
  elif c=='{':depth+=1
  elif c=='}':
   depth-=1
   if not depth:return text[begin:i+1]
  i+=1
 raise AssertionError(signature)
paths=[s/x for x in ('Tab5Map.cpp','Tab5Map.h','Tab5GeoIndex.h','Tab5GeoFold.h','Tab5MapViewPolicy.h','Tab5MapViewState.h')]+[s/'sdcard/System/maps/gazetteer/cities.t5geo'];sourceHashes={str(p.relative_to(r)):sha(p) for p in paths}
signatures=['bool Tab5MapEngine::createQueues(', 'void Tab5MapEngine::discardSearchRequests(', 'bool Tab5MapEngine::requestSearch(', 'bool Tab5MapEngine::pollSearch(', 'void Tab5MapEngine::releaseSearch(', 'bool Tab5MapEngine::poll(', 'void Tab5MapEngine::release(', 'void Tab5MapEngine::taskEntry(', 'void Tab5MapEngine::taskLoop(', 'bool Tab5MapEngine::localSearch(', 'size_t Tab5MapEngine::markerCount(', 'bool Tab5MapEngine::marker(', 'void Tab5MapEngine::beginGeneration(', 'bool Tab5MapEngine::requestTile(']
fixed=r'''
 // With usable Wi-Fi, a default request calls Internet first and never reads SD on a match.
 {auto before=ioBytes.load();auto t=start();CHECK(e.requestSearch("Lorient",100,47.7,-3.3));auto r=waitResult(e,100);CHECK(r.success&&!r.local&&r.count==1&&httpCalls==1);CHECK(ioBytes==before);e.releaseSearch(r);stop(t);}
 // Service failure, empty successful response and a disconnect all fall back.
 for(int mode=1;mode<=3;++mode){httpMode=mode;wifiConnected=true;auto calls=httpCalls.load();auto t=start();CHECK(e.requestSearch("Lorient",100+mode,47.7,-3.3));auto r=waitResult(e,100+mode);CHECK(httpCalls==calls+1);CHECK(r.success&&r.local&&r.gazetteerAvailable&&r.count);bool saved=false,meshFound=false;for(unsigned i=0;i<r.count;++i){saved|=r.items[i].kind==Tab5MapMarkerKind::Saved;meshFound|=r.items[i].kind==Tab5MapMarkerKind::Mesh;}CHECK(saved&&meshFound);e.releaseSearch(r);stop(t);}
 httpMode=0;
 // An offline default does not turn Wi-Fi on. A queued preference can use a
 // connection established before the worker starts, without repeating UI input.
 {networkEnabled=false;wifiConnected=false;auto calls=httpCalls.load();auto t=start();CHECK(e.requestSearch("Paris",110,48.8,2.3));auto r=waitResult(e,110);CHECK(r.success&&r.local&&httpCalls==calls);e.releaseSearch(r);stop(t);}
 {e._active=true;networkEnabled=true;wifiConnected=false;auto calls=httpCalls.load();CHECK(e.requestSearch("Paris",111,48.8,2.3));wifiConnected=true;auto t=start();auto r=waitResult(e,111);CHECK(r.success&&!r.local&&httpCalls==calls+1);e.releaseSearch(r);stop(t);}
 // A foreground radio lease makes Internet unavailable; return local results
 // without waiting for a long-playing stream or closing its shared transport.
 {networkYield=true;auto calls=httpCalls.load();auto t=start();CHECK(e.requestSearch("Lorient",112,47.7,-3.3));auto r=waitResult(e,112);CHECK(r.success&&r.local&&httpCalls==calls);e.releaseSearch(r);stop(t);networkYield=false;}
 // Superseding an in-flight HTTP failure cannot start obsolete local I/O or
 // publish the old result; the new offline query owns the complete marker copy.
 {blockHttp=true;httpEntered=false;httpMode=1;auto reads=ioBytes.load();auto calls=httpCalls.load();auto t=start();CHECK(e.requestSearch("Paris",120,48.8,2.3));CHECK(waitFor([&]{return httpEntered.load();}));wifiConnected=false;CHECK(e.requestSearch("Lorient",121,47.7,-3.3));CHECK(ioBytes==reads);blockHttp=false;auto r=waitResult(e,121);CHECK(r.success&&r.local&&httpCalls==calls+1);CHECK(!uxQueueMessagesWaiting(&sr));e.releaseSearch(r);stop(t);wifiConnected=true;httpMode=0;}
 // Deactivation after a blocked remote request releases partial online payloads
 // without opening a local file and leaves no response queue/marker snapshot.
 {blockHttp=true;httpEntered=false;httpMode=1;auto reads=ioBytes.load();auto t=start();CHECK(e.requestSearch("Paris",122,48.8,2.3));CHECK(waitFor([&]{return httpEntered.load();}));stop(t);CHECK(ioBytes==reads);CHECK(!uxQueueMessagesWaiting(&sr));httpMode=0;}
 // Changing viewport generations while a tile downloads never blocks request
 // submission. The stale worker allocation is discarded; only latest arrives.
 {e._active=true;e.beginGeneration(10);blockTile=true;tileEntered=false;auto t=start();CHECK(e.requestTile(4,1,1,10));CHECK(waitFor([&]{return tileEntered.load();}));for(unsigned g=11;g<=60;++g)e.beginGeneration(g);CHECK(e.requestTile(4,2,2,60));blockTile=false;CHECK(waitFor([&]{return uxQueueMessagesWaiting(&tr)!=0;}));Tab5MapTileResult pending;CHECK(e.poll(pending));CHECK(pending.generation==60&&pending.x==2&&pending.data);CHECK(pending.data[0]==60);auto bytes=allocated;for(unsigned g=61;g<=100;++g)e.beginGeneration(g);CHECK(allocated==bytes&&pending.data[0]==60);e.release(pending);CHECK(allocated==base);CHECK(e.requestTile(4,3,3,100));CHECK(waitFor([&]{return uxQueueMessagesWaiting(&tr)!=0;}));e.beginGeneration(101);CHECK(allocated==base);stop(t);}
 // A blocked local search keeps its own cancellation ID. Zooming changes the
 // tile generation but does not invalidate this separate search or its file.
 {wifiConnected=false;blockRead=true;readEntered=false;auto t=start();CHECK(e.requestSearch("Paris",130,48.8,2.3));CHECK(waitFor([&]{return readEntered.load();}));for(unsigned g=102;g<=150;++g)e.beginGeneration(g);blockRead=false;auto r=waitResult(e,130);CHECK(r.success&&r.local&&r.count);e.releaseSearch(r);stop(t);wifiConnected=true;}
 // A superseded local request releases the actual bounded gazetteer reader.
 {wifiConnected=false;blockRead=true;readEntered=false;auto t=start();CHECK(e.requestSearch("sa",131,0,0));CHECK(waitFor([&]{return readEntered.load();}));CHECK(e.requestSearch("Lorient",132,47.7,-3.3));blockRead=false;auto r=waitResult(e,132);CHECK(r.success&&r.local);e.releaseSearch(r);stop(t);wifiConnected=true;}
'''
baseline=r'''
 {auto t=start();CHECK(e.requestSearch("Lorient",100,47.7,-3.3));auto r=waitResult(e,100);CHECK(r.success&&r.local&&httpCalls==0);e.releaseSearch(r);stop(t);}
 {httpMode=1;auto t=start();CHECK(e.requestSearch("Paris",101,48.8,2.3,true));auto r=waitResult(e,101);CHECK(!r.success&&!r.local&&r.httpCode==503&&httpCalls==1);e.releaseSearch(r);stop(t);}
'''
def run(label,src,header,scenario):
 header='\n'.join(x for x in header.splitlines() if not x.startswith('#include') and not x.startswith('#pragma')).replace(' private:',' public:')
 functions=extract(src,'class GazetteerReader {')+';\n'+'\n'.join(extract(src,x) for x in signatures)
 file=b/(label+'.cpp');file.write_text((here/'worker.cpp').read_text().replace('/* HEADER */',header).replace('/* FUNCTIONS */',functions).replace('// SCENARIOS',scenario))
 exe=b/label;cmd=['g++','-std=c++17','-O1','-g','-pthread','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(s),str(file),'-o',str(exe)]
 p=subprocess.run(cmd,capture_output=True,text=True);(b/(label+'-build.log')).write_text(p.stdout+p.stderr)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 p=subprocess.run([str(exe),str(s/'sdcard/System/maps/gazetteer/cities.t5geo')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'),timeout=60);print(label,p.stdout+p.stderr);(b/(label+'-run.log')).write_text(p.stdout+p.stderr)
 if p.returncode:raise RuntimeError(label+' failed')
 return {'passed':True,'checks':int(re.search(r'checks=(\d+) PASS',p.stdout)[1]),'output':p.stdout.strip()}
before=None
if a.baseline_zip:
 with zipfile.ZipFile(a.baseline_zip) as z:
  name=next(x for x in z.namelist() if x.endswith('/Tab5Map.cpp') and '/Tab5OS_Arduino_' in x and '/tools/' not in x);src=z.read(name).decode();header=z.read(name[:-3]+'h').decode()
  before=run('baseline',src,header,baseline)
current=run('current',(s/'Tab5Map.cpp').read_text(),(s/'Tab5Map.h').read_text(),fixed)
assert sourceHashes=={str(p.relative_to(r)):sha(p) for p in paths},'sources changed'
proof=r/'tools/verification/maps-worker-0845';proof.mkdir(exist_ok=True)
for f in b.glob('*.log'):(proof/f.name).write_bytes(f.read_bytes())
report={'version':'0.8.45','passed':True,'physicalDeviceTested':False,'baseline':before,'current':current,'sourceHashes':sourceHashes,'testSourceHashes':{str(p.relative_to(r)):sha(p) for p in here.iterdir() if p.is_file()},'evidenceHashes':{str(p.relative_to(r)):sha(p) for p in proof.iterdir() if p.is_file()},'method':__doc__,'limits':['Production requestSearch/taskEntry/taskLoop/localSearch/reader/queue creation/beginGeneration/requestTile/poll/release methods extracted verbatim. Real shipped cities index parsed in bounded pages.','HTTP outcomes, File, queue, mutex, allocation and radio boundaries are host adapters, not real FreeRTOS/SD/Wi-Fi/TLS. File/VFS ownership separately tested with actual SDK implementation.','No GUI/hardware zoom latency or device SD fault claimed. ASan/UBSan active; LSan disabled; tracked capability allocations/files/storage leases checked zero after sessions.']}
(r/'tools/verification/maps-worker-0845.json').write_text(json.dumps(report,indent=2)+'\n')
