#!/usr/bin/env python3
"""Run the actual publisher/readers with only String, RTOS mutex and device boundaries adapted."""
from pathlib import Path
import hashlib,json,os,re,subprocess,zipfile,argparse
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[2];s=next(r.glob('Tab5OS_Arduino_*_Ecriture'))
b=r.parent.parent/'build_0843'/'audio-events-host';b.mkdir(parents=True,exist_ok=True)
source=(s/'Tab5Audio.cpp').read_text();header=(s/'Tab5Audio.h').read_text()
if a.baseline_zip:
 with zipfile.ZipFile(a.baseline_zip) as z:
  f=next(n for n in z.namelist() if n.endswith('/Tab5Audio.cpp'));source=z.read(f).decode();header=z.read(f[:-len('Tab5Audio.cpp')]+'Tab5Audio.h').decode()
def block(text,start):
 pos=text.index(start);brace=text.index('{',pos);depth=0
 for end in range(brace,len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if not depth:return text[pos:end+1]
 raise AssertionError(start)
new='bool Tab5AudioEngine::indicatorSnapshot(' in source
names=['bool Tab5AudioEngine::snapshot(', 'void Tab5AudioEngine::publishSnapshot(', 'bool Tab5AudioEngine::takeUiDirty(']
if new:names+=['bool Tab5AudioEngine::indicatorSnapshot(', 'bool Tab5AudioEngine::wifiSnapshot(']
methods='\n'.join(block(source,n) for n in names)
structs=block(header,'enum class Tab5AudioMode')+';\n'+block(header,'struct Tab5AudioSnapshot')+';'
fixture=r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
struct String {
 std::string value;static size_t copies;static int failAfter;
 String()=default;String(const char* s):value(s){}
 String(const String& r):value(r.value){++copies;}String(String&&)=default;
 String& operator=(const String& r){++copies;if(failAfter==0){value.clear();failAfter=-1;return *this;}if(failAfter>0)--failAfter;value=r.value;return *this;}String& operator=(String&&)=default;
 String& operator=(const char* s){value=s;return *this;}
 bool operator!=(const String& r)const{return value!=r.value;}
 bool operator==(const char* r)const{return value==r;}
 size_t length()const{return value.size();}
};size_t String::copies=0;int String::failAfter=-1;
struct Mutex {bool blocked=false;unsigned calls=0,held=0;int timeout=0;};
using SemaphoreHandle_t=Mutex*;constexpr int pdTRUE=1,portMAX_DELAY=-1;
int xSemaphoreTake(Mutex* m,int timeout){++m->calls;m->timeout=timeout;if(m->blocked)return 0;assert(!m->held);++m->held;return pdTRUE;}
void xSemaphoreGive(Mutex* m){assert(m->held==1);--m->held;}
uint32_t clockMs=0;uint32_t millis(){return clockMs;}
constexpr int WL_CONNECTED=3;struct{int state=0;int status(){return state;}}WiFi;
struct Local{uint32_t size=100,pos=0;uint32_t getSize(){return size;}uint32_t getPos(){return pos;}};
struct Radio{size_t bufferedBytes(){return 100;}size_t capacityBytes(){return 1000;}};
// STRUCTS
class Tab5AudioEngine {public:
 SemaphoreHandle_t _snapshotMutex=nullptr;Tab5AudioSnapshot _snapshot;
 std::atomic<uint32_t> _indicatorRevision{1};
 bool _uiDirty=true,_snapshotDirty=true,_paused=false,_wifiEnabled=false,_wifiPending=false,_wifiAutoStart=false,decoding=false;
 uint32_t _lastSnapshotAt=0,_playStartedAt=0,_pauseStartedAt=0,_localEstimatedSeconds=0,_recordDataBytes=0;
 uint8_t _volume=150;uint16_t _recordLevel=0;int _wifiRssiCached=-42;unsigned _stationIndex=0;
 static constexpr unsigned RECORD_SAMPLE_RATE=16000;
 Tab5AudioMode _mode=Tab5AudioMode::Idle;Local* _localSource=nullptr;Radio* _radioSource=nullptr;
 String _nowPlaying,_status,_lastRecordingPath,_currentLocalPath,_wifiSsid;
 struct Station{String url;};std::vector<Station> _stations;
 bool decoderRunning(){return decoding;}
 bool snapshot(Tab5AudioSnapshot&)const;bool takeUiDirty();void publishSnapshot();
 bool indicatorSnapshot(Tab5AudioSnapshot&,uint32_t&,bool=false)const;bool wifiSnapshot(bool&,bool&)const;
};
// METHODS
void publish(Tab5AudioEngine& e){clockMs+=50;e.publishSnapshot();}
int main(){
 Mutex lock;Tab5AudioEngine e;e._snapshotMutex=&lock;e.publishSnapshot();
 // Reference: repeat the unchanged home indicator for60s. Then allow the new
 // observer to be called every8ms (instead of the former250ms timer).
 Tab5AudioSnapshot display;uint32_t seen=0;(void)seen;unsigned copies=0,lockCalls=0,readerLocks=0;
 // INITIAL_READ
 String::copies=0;lock.calls=0;
 for(unsigned t=0;t<60000;t+=2){clockMs=t;e.publishSnapshot();if(t%STEP==0){auto calls=lock.calls;READ;readerLocks+=lock.calls-calls;}}
 copies=String::copies;lockCalls=lock.calls;
 // EXTRA_TESTS
 assert(!lock.held);
 std::cout<<"{\"passed\":true,\"durationMs\":60000,\"readerStepMs\":STEP,\"idleCopies\":"<<copies<<",\"mutexCallsIncludingPublisher\":"<<lockCalls<<",\"readerMutexCalls\":"<<readerLocks<<",\"physicalDeviceTested\":false}"<<std::endl;
}
'''
extra=r'''
 // Independent observers cannot steal each other's event; legacy dirty consumer remains independent.
 uint32_t second=0;Tab5AudioSnapshot other;
 assert(e.indicatorSnapshot(other,second));assert(second==seen);assert(!e.indicatorSnapshot(other,second));
 e._nowPlaying="track";publish(e);assert(e.takeUiDirty()==false); // _uiDirty was consumed by first publish, unchanged here.
 assert(e.indicatorSnapshot(display,seen));assert(display.nowPlaying=="track");assert(e.indicatorSnapshot(other,second));
 auto event=[&](){publish(e);assert(e.indicatorSnapshot(display,seen));assert(!e.indicatorSnapshot(display,seen));};
 e._mode=Tab5AudioMode::Local;e._currentLocalPath="/Audio/one.wav";event();assert(display.currentPath=="/Audio/one.wav");
 e._paused=true;event();assert(display.paused);
 e._volume=96;event();assert(display.volume==96);
 e._nowPlaying="different";event();assert(display.nowPlaying=="different");
 e._mode=Tab5AudioMode::Radio;e._stations.push_back({String("https://one")});event();assert(display.currentStationUrl=="https://one");
 e._stations[0].url="https://two";event();assert(display.currentStationUrl=="https://two");
 e._mode=Tab5AudioMode::Idle;event();assert(display.currentStationUrl.length()==0);
 // Progress/status/RSSI/buffer changes stay available through snapshot(), without
 // invalidating a static title indicator. Force and full snapshot copy latest values.
 e._status="status";e._wifiRssiCached=-20;e._recordLevel=99;publish(e);assert(!e.indicatorSnapshot(display,seen));
 assert(e.indicatorSnapshot(display,seen,true));assert(display.status=="status");
 e._uiDirty=true;publish(e);assert(e.takeUiDirty());assert(!e.takeUiDirty());assert(!e.indicatorSnapshot(display,seen));
 // Real producer progress/recording/buffer fields continue changing without
 // causing the title-only observer to recopy them.
 e._mode=Tab5AudioMode::Local;e._paused=false;Local local;e._localSource=&local;e._playStartedAt=clockMs-1000;event();
 local.pos=50;publish(e);assert(!e.indicatorSnapshot(display,seen));Tab5AudioSnapshot full;assert(e.snapshot(full));assert(full.playbackProgressPermille==500);
 e._mode=Tab5AudioMode::Radio;event();Radio radio;e._radioSource=&radio;publish(e);assert(!e.indicatorSnapshot(display,seen));assert(e.snapshot(full));assert(full.radioBufferedBytes==100);
 e._mode=Tab5AudioMode::Recording;event();e._recordDataBytes=32000;publish(e);assert(!e.indicatorSnapshot(display,seen));assert(e.snapshot(full));assert(full.recordingSeconds==1);
 // A busy or unavailable mutex never blocks or consumes an event or output.
 e._volume=42;publish(e);const auto oldSeen=seen;lock.blocked=true;
 String::copies=0;assert(!e.indicatorSnapshot(display,seen));assert(lock.timeout==0);assert(seen==oldSeen&&String::copies==0);assert(display.volume==96);
 lock.blocked=false;assert(e.indicatorSnapshot(display,seen));assert(display.volume==42);
 bool enabled=false,connected=false;e._wifiEnabled=true;WiFi.state=WL_CONNECTED;publish(e);
 String::copies=0;assert(e.wifiSnapshot(enabled,connected));assert(enabled&&connected&&String::copies==0);
 lock.blocked=true;enabled=false;connected=false;assert(!e.wifiSnapshot(enabled,connected));assert(!enabled&&!connected&&lock.timeout==0);lock.blocked=false;
 e._snapshotMutex=nullptr;assert(!e.indicatorSnapshot(display,seen,true));assert(!e.wifiSnapshot(enabled,connected));e._snapshotMutex=&lock;
 // Failed copies of each relevant metadata field leave the event unconsumed.
 e._nowPlaying="long title";e._currentLocalPath="/Audio/new.wav";e._mode=Tab5AudioMode::Radio;publish(e);
 for(int failed:{0,3,4}){auto before=seen;String::failAfter=failed;assert(!e.indicatorSnapshot(display,seen,true));assert(seen==before&&String::failAfter==-1);assert(e.indicatorSnapshot(display,seen,true));}
 // Fast unchanged check must avoid the mutex entirely, including contention.
 auto calls=lock.calls;assert(!e.indicatorSnapshot(display,seen));assert(lock.calls==calls);
 // Wrap never publishes reserved initial0.
 e._indicatorRevision.store(UINT32_MAX);seen=UINT32_MAX;e._paused=!e._paused;publish(e);assert(e._indicatorRevision.load()==1);assert(e.indicatorSnapshot(display,seen));
 // Force retries do not consume version on contention.
 lock.blocked=true;assert(!e.indicatorSnapshot(display,seen,true));lock.blocked=false;assert(e.indicatorSnapshot(display,seen,true));
'''
if new:
 initial='assert(e.indicatorSnapshot(display,seen));'
 read='e.indicatorSnapshot(display,seen)';step='8'
 # Consume independent legacy dirty state before the non-consuming tests.
 extra='assert(e.takeUiDirty());assert(!e.takeUiDirty());\n'+extra
else:initial='assert(e.snapshot(display));';read='e.snapshot(display)';step='250';extra=''
fixture=fixture.replace('// STRUCTS',structs).replace('// METHODS',methods).replace('// INITIAL_READ',initial).replace('// EXTRA_TESTS',extra).replace('STEP',step).replace('READ;',read+';')
file=b/('before.cpp' if a.baseline_zip else 'after.cpp');file.write_text(fixture);exe=file.with_suffix('')
cmd=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(file),'-o',str(exe)]
build=subprocess.run(cmd,capture_output=True,text=True);file.with_suffix('.build.log').write_text(build.stdout+build.stderr);print(build.stderr[-6000:]);build.check_returncode()
run=subprocess.run([str(exe)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));file.with_suffix('.run.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr);run.check_returncode()
metrics=json.loads(run.stdout.strip());assert metrics['idleCopies']==(0 if new else 1440)
report={'version':'0.8.42' if a.baseline_zip else '0.8.43',**metrics,'scope':'Exact production publishSnapshot and observer methods with counted host String and mutex/device boundary adapters. No FreeRTOS task, radio, physical heap or device frame timing measured. ASan/UBSan; LSan disabled in this execution environment.','methods':{name:hashlib.sha256(block(source,name).encode()).hexdigest() for name in names},'sourceHashes':{'Tab5Audio.cpp':hashlib.sha256(source.encode()).hexdigest(),'Tab5Audio.h':hashlib.sha256(header.encode()).hexdigest()},'testSourceSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
o=r/'tools/audit-0843';o.mkdir(exist_ok=True);(o/('audio-events-before.json' if a.baseline_zip else 'audio-events-after.json')).write_text(json.dumps(report,indent=2)+'\n')
