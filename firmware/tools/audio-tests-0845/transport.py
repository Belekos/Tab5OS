#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,os,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--sdk',type=Path,required=True);p.add_argument('--baseline-zip',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parents[1];sketch=next(r.glob('Tab5OS_Arduino_*_Ecriture'));build=r.parents[1]/'build_0845/audio-transport';build.mkdir(parents=True,exist_ok=True)
sdk=a.sdk.read_text();stream=(a.sdk.parents[3]/'cores/esp32/Stream.cpp').read_text();src=(sketch/'Tab5Audio.cpp').read_text()
with zipfile.ZipFile(a.baseline_zip) as z:baseline=z.read(next(n for n in z.namelist() if n.endswith('/Tab5Audio.cpp'))).decode()
def block(s,n):
 start=s.index(n);brace=s.index('{',start);depth=0
 for end in range(brace,len(s)):
  if s[end]=='{':depth+=1
  elif s[end]=='}':
   depth-=1
   if not depth:return s[start:end+1]
 raise ValueError(n)
http='\n'.join(block(sdk,n) for n in ['HTTPClient::~HTTPClient()', 'void HTTPClient::end(', 'void HTTPClient::disconnect(', 'bool HTTPClient::connected(', 'NetworkClient *HTTPClient::getStreamPtr('])
fixture=(here/'transport_fixture.cpp').read_text();basecoop=(sketch/'Tab5Cooperate.h').read_text();baseclient=(sketch/'Tab5CooperativeClient.h').read_text();baseclient='\n'.join(x for x in baseclient.splitlines() if not x.startswith('#include') and x not in ['#ifdef ARDUINO','#endif','#pragma once'])
fixture=fixture.replace('// SDK_STREAM_METHODS',block(stream,'int Stream::timedRead()')).replace('// SDK_HTTP_METHODS',http).replace('// PRODUCTION_COOPERATE',block(basecoop,'class Tab5TimeSlice')+';').replace('// PRODUCTION_CLIENT',baseclient)
results={}
for name,text in [('before',baseline),('after',src)]:
 f=fixture.replace('// PRODUCTION_SOURCE',block(text,'class Tab5HttpAudioSource')+';').replace('// PRODUCTION_BUFFERING',block(text,'class Tab5PsramStream')+';\n'+block(text,'class Tab5BufferedRadioSource')+';')
 if name=='before':
  f=f.replace('// BASELINE_SCENARIO','{fragmentedHeaders=true;yieldCount=0;Tab5HttpAudioSource source;assert(source.open("http://radio"));assert(yieldCount==0);fragmentedHeaders=false;source.close();}\n{Tab5HttpAudioSource source;assert(source.open("http://radio"));peerConnected=false;source.close();source.close();}\nreturn 0;')
 cpp=build/(name+'.cpp');cpp.write_text(f);exe=cpp.with_suffix('')
 proc=subprocess.run(['g++','-std=c++17','-O1','-g','-pthread','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie',str(cpp),'-o',str(exe)],capture_output=True,text=True);(build/(name+'-build.log')).write_text(proc.stdout+proc.stderr);print(proc.stderr[-5000:]);proc.check_returncode()
 proc=subprocess.run([str(exe)],capture_output=True,text=True,timeout=120,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(build/(name+'-run.log')).write_text(proc.stdout+proc.stderr)
 if name=='before':assert proc.returncode!=0 and 'heap-use-after-free' in proc.stderr;results[name]={'heapUseAfterFreeReproduced':True};print('Baseline use-after-free reproduced')
 else:print(proc.stdout+proc.stderr);proc.check_returncode();results[name]=json.loads(proc.stdout)
proof=r/'tools/verification/audio-transport-0845.json';proof.parent.mkdir(exist_ok=True)
report={'version':'0.8.45','passed':True,'physicalDeviceTested':False,**results,'sourceHashes':{str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [sketch/'Tab5Audio.cpp',sketch/'Tab5CooperativeClient.h',sketch/'Tab5Cooperate.h']},'testSourceHashes':{str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [here/'transport.py',here/'transport_fixture.cpp']},'sdkSha256':hashlib.sha256(a.sdk.read_bytes()).hexdigest(),'scope':'Complete production Tab5HttpAudioSource, Tab5BufferedRadioSource, Tab5PsramStream; actual SDK HTTP destructor/end/disconnect/connected/getStreamPtr and Stream::timedRead. RTOS/net/heap boundaries controlled, producer uses host thread. ASan/UBSan active; LSan unavailable.'}
proof.write_text(json.dumps(report,indent=2)+'\n')
