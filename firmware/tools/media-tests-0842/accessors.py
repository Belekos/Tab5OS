#!/usr/bin/env python3
"""Compile exact snapshot/accessor methods; count String copies with a host adapter."""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, tempfile, zipfile
root=Path(__file__).resolve().parents[2]
sketch=root/'Tab5OS_Arduino_v0.8.42_Ecriture'
p=argparse.ArgumentParser();p.add_argument('--baseline-zip');p.add_argument('--build');args=p.parse_args()
build=Path(args.build) if args.build else Path(tempfile.mkdtemp(prefix='tab5-accessors-'))
build.mkdir(parents=True,exist_ok=True)
if args.baseline_zip:
 with zipfile.ZipFile(args.baseline_zip) as z:
  prefix=next(n[:-len('Tab5Audio.cpp')] for n in z.namelist() if n.endswith('/Tab5Audio.cpp'))
  source=z.read(prefix+'Tab5Audio.cpp').decode();header=z.read(prefix+'Tab5Audio.h').decode()
else:source=(sketch/'Tab5Audio.cpp').read_text();header=(sketch/'Tab5Audio.h').read_text()
def block(text,start):
 pos=text.index(start);brace=text.index('{',pos);depth=0
 for end in range(brace,len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if not depth:return text[pos:end+1]
 raise AssertionError(start)
names=['hasSavedWifi','savedWifiSsid','wifiEnabled','wifiAutoStart','wifiConnecting',
 'playing','wifiConnected','wifiRssi','recordingSeconds','playbackProgressPermille',
 'playbackElapsedSeconds','playbackEstimatedSeconds','volume','mode','paused','recording',
 'recordingLevel','nowPlaying','status','lastRecordingPath','currentPath']
methods=[];decls=[];types={}
for name in ['snapshot']+names:
 match=re.search(r'^(\w+) Tab5AudioEngine::'+name+r'\(',source,re.M);assert match
 text=block(source,match.group());methods.append(text)
 decls.append(text[:text.index('{')].replace('Tab5AudioEngine::','').strip()+';')
 types[name]=match[1]
helper=block(source,'template <typename T>') if 'T audioSnapshotValue(' in source else ''
structs=block(header,'enum class Tab5AudioMode')+';\n'+block(header,'struct Tab5AudioSnapshot')+';'
fixture=r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <iostream>
#include <utility>
struct String {
 std::string value;
 static size_t copies,bytes;
 String()=default;String(const char* v):value(v){}
 String(const String& rhs):value(rhs.value){++copies;bytes+=value.size();}
 String& operator=(const String& rhs){value=rhs.value;++copies;bytes+=value.size();return *this;}
 String(String&&)=default;String& operator=(String&&)=default;
};size_t String::copies=0;size_t String::bytes=0;
struct Mutex{bool fail=false;int held=0;};
using SemaphoreHandle_t=Mutex*;
constexpr int pdTRUE=1,portMAX_DELAY=-1;
int xSemaphoreTake(Mutex* m,int){if(m->fail)return 0;assert(!m->held);++m->held;return 1;}
void xSemaphoreGive(Mutex* m){assert(m->held==1);--m->held;}
// STRUCTS
class Tab5AudioEngine {public:
 Mutex* _snapshotMutex=nullptr;Tab5AudioSnapshot _snapshot;
 // DECLS
};
// METHODS
int main(){
 Tab5AudioEngine engine;Mutex mutex;engine._snapshotMutex=&mutex;
 // INIT
 size_t scalarCopies=0,stringCopies=0,scalarBytes=0;
 String::copies=String::bytes=0;
 for(int i=0;i<10000;++i){
 // SCALAR_CHECKS
 }
 scalarCopies=String::copies;scalarBytes=String::bytes;
 String::copies=String::bytes=0;
 // STRING_CHECKS
 stringCopies=String::copies;
 String::copies=0;Tab5AudioSnapshot whole;assert(engine.snapshot(whole));assert(String::copies==6);
 for(int unavailable=0;unavailable<2;++unavailable){
 engine._snapshotMutex=unavailable?nullptr:&mutex;mutex.fail=true;
 // FALLBACK_CHECKS
 }
 assert(mutex.held==0);
 std::cout<<"{\"scalarIterations\":10000,\"scalarGetterCount\":SCALAR_COUNT,\"scalarStringCopies\":"<<scalarCopies<<",\"scalarCopiedBytes\":"<<scalarBytes<<",\"stringGetterCount\":STRING_COUNT,\"stringGetterCopies\":"<<stringCopies<<",\"fallbackCasesPassed\":true}"<<std::endl;
}
'''
init=[];scalars=[];strings=[];fallback=[]
for name in names:
 typ=types[name]
 if typ=='String':
  init.append(f'engine._snapshot.{name}=String("{name}-fixture-data-longer-than-small-string");')
  strings.append(f'assert(engine.{name}().value==engine._snapshot.{name}.value);')
  fallback.append(f'assert(engine.{name}().value.empty());')
 else:
  value='Tab5AudioMode::Radio' if typ=='Tab5AudioMode' else ('true' if typ=='bool' else ('-72' if typ=='int' else '123'))
  init.append(f'engine._snapshot.{name}={value};')
  scalars.append(f'assert(engine.{name}()==engine._snapshot.{name});')
  fallback.append(f'assert(engine.{name}()=='+('Tab5AudioMode::Idle' if typ=='Tab5AudioMode' else '0')+');')
init.append('engine._snapshot.currentStationUrl=String("https://fixture.invalid/long-stream-path-for-copy-counting");')
for token,value in {'// STRUCTS':structs,'// DECLS':'\n'.join(decls),'// METHODS':helper+'\n'+'\n'.join(methods),'// INIT':'\n'.join(init),'// SCALAR_CHECKS':'\n'.join(scalars),'// STRING_CHECKS':'\n'.join(strings),'// FALLBACK_CHECKS':'\n'.join(fallback),'SCALAR_COUNT':str(len(scalars)),'STRING_COUNT':str(len(strings))}.items():fixture=fixture.replace(token,value)
(build/'test.cpp').write_text(fixture)
subprocess.run(['g++','-std=c++17','-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined',str(build/'test.cpp'),'-o',str(build/'test')],check=True)
import os
run=subprocess.run([str(build/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
print(run.stdout+run.stderr,end='');run.check_returncode()
metrics=json.loads(run.stdout.strip())
if not args.baseline_zip:assert metrics['scalarStringCopies']==0 and metrics['stringGetterCopies']==len(strings)
report={'version':'0.8.41' if args.baseline_zip else '0.8.42','passed':True,**metrics,'sourceSha256':hashlib.sha256(source.encode()).hexdigest(),'scope':'Exact production accessor/snapshot methods and snapshot declaration with host String copy counter and mutex adapter; values, mutex release, initialization/error fallbacks; ASan+UBSan. This counts copies, not physical P4 allocations or latency.'}
out=root/'tools/audit-0842'/('audio-accessors-before.json' if args.baseline_zip else 'audio-accessors-after.json');out.write_text(json.dumps(report,indent=2)+'\n');print(out)
