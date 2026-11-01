#!/usr/bin/env python3
"""Run exact main map routing predicates with SDK event-cache and audio boundary adapters."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[2];s=next(r.glob('Tab5OS_Arduino_*_Ecriture'));main=next(s.glob('*.ino'));src=main.read_text()
if a.baseline_zip:
 with zipfile.ZipFile(a.baseline_zip) as z:src=z.read(next(x for x in z.namelist() if x.endswith('.ino') and '/Tab5OS_Arduino_' in x)).decode()
def block(text,start):
 pos=text.index(start);brace=text.index('{',pos);depth=0
 for end in range(brace,len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if not depth:return text[pos:end+1]
 raise AssertionError(start)
new='static bool mapNetworkConnected()' in src
names=['static bool mapDownloadsAllowed(']
if new:names.insert(0,'static bool mapNetworkConnected()')
fixture=r'''
#include <cassert>
#include <cstdint>
#include <iostream>
enum class Tab5AudioMode{Idle,Local,Radio,Recording};
struct Tab5AudioRoutingSnapshot{Tab5AudioMode mode;bool paused,playing,wifiConnected;uint32_t radioBufferedBytes,radioBufferCapacity;};
struct Tab5AudioSnapshot{Tab5AudioMode mode=Tab5AudioMode::Idle;bool wifiConnected=false;};
constexpr int WL_CONNECTED=3;
struct{int state=0,calls=0;int status(){++calls;return state;}}WiFi;
bool permission=true;bool tab5NetworkEnabled(){return permission;}
struct Engine{Tab5AudioMode mode=Tab5AudioMode::Idle;bool ready=true,connected=false;unsigned stringCopies=0,scalarCopies=0;
 bool snapshot(Tab5AudioSnapshot& s){if(!ready)return false;s.mode=mode;s.wifiConnected=connected;stringCopies+=6;return true;}
 bool routingSnapshot(Tab5AudioRoutingSnapshot& s){if(!ready)return false;s.mode=mode;s.wifiConnected=connected;++scalarCopies;return true;}
}Tab5Audio;
//METHODS
int main(){
 unsigned checks=0;bool connected=false;
 for(auto mode:{Tab5AudioMode::Idle,Tab5AudioMode::Local,Tab5AudioMode::Radio,Tab5AudioMode::Recording}){
  Tab5Audio.mode=mode;
  for(bool cached:{false,true})for(bool actual:{false,true})for(bool allowed:{false,true}){
   Tab5Audio.connected=cached;WiFi.state=actual?WL_CONNECTED:0;permission=allowed;
   assert(mapDownloadsAllowed(&connected)==(mode!=Tab5AudioMode::Local&&mode!=Tab5AudioMode::Recording));
   assert(connected==EXPECTED);++checks;
  }
 }
 Tab5Audio.ready=false;assert(!mapDownloadsAllowed(nullptr));++checks;
 Tab5Audio.ready=true;Tab5Audio.mode=Tab5AudioMode::Idle;Tab5Audio.stringCopies=0;Tab5Audio.scalarCopies=0;
 for(unsigned i=0;i<1000;++i){mapDownloadsAllowed(&connected);}
 assert(Tab5Audio.stringCopies==COPIES);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"calls\":1000,\"fullStringCopies\":"<<Tab5Audio.stringCopies<<",\"routingCopies\":"<<Tab5Audio.scalarCopies<<",\"usesSdkConnectionState\":NEW,\"physicalDeviceTested\":false}"<<std::endl;
}
'''.replace('//METHODS','\n'.join(block(src,n) for n in names)).replace('EXPECTED','(permission&&actual)' if new else 'cached').replace('COPIES','0' if new else '6000').replace('NEW','true' if new else 'false')
b=r.parent.parent/'build_0844'/'map-routing';b.mkdir(parents=True,exist_ok=True);cpp=b/('before.cpp' if a.baseline_zip else 'after.cpp');cpp.write_text(fixture);exe=cpp.with_suffix('')
cmd=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(cpp),'-o',str(exe)]
subprocess.run(cmd,check=True)
run=subprocess.run([str(exe)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));print(run.stdout+run.stderr);run.check_returncode()
report=json.loads(run.stdout);report.update(version='0.8.43' if a.baseline_zip else '0.8.44',scope='Exact mapDownloadsAllowed and new mapNetworkConnected; cached SDK status and audio adapters. Covers stale audio network snapshot in both directions; not an actual WiFi disconnect/reconnect test.',methods={n:hashlib.sha256(block(src,n).encode()).hexdigest() for n in names},sourceSha256=hashlib.sha256(src.encode()).hexdigest())
o=r/'tools/audit-0844';o.mkdir(exist_ok=True);(o/('map-routing-before.json' if a.baseline_zip else 'map-routing-after.json')).write_text(json.dumps(report,indent=2)+'\n')

if not a.baseline_zip:
 report['sourceHashes']={str(main.relative_to(r)):hashlib.sha256(src.encode()).hexdigest()}
 report['testSourceHashes']={str(Path(__file__).relative_to(r)):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
 (r/'tools/verification/map-routing-0844.json').write_text(json.dumps(report,indent=2)+'\n')
