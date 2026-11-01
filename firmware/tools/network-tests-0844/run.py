#!/usr/bin/env python3
"""Exercise real network helper sources against explicit host SDK boundaries."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
here=Path(__file__).resolve().parent
sketch=root/'Tab5OS_Arduino_v0.8.44_Ecriture'
p=argparse.ArgumentParser();p.add_argument('--build');p.add_argument('--sdk-core');args=p.parse_args()
build=Path(args.build).resolve() if args.build else Path(tempfile.mkdtemp(prefix='tab5-network-0844-'))
build.mkdir(parents=True,exist_ok=True)
for name in ['Tab5CooperativeClient.h','Tab5Cooperate.h','Tab5NetDiagnostics.cpp','Tab5NetDiagnostics.h']:
 (build/name).write_bytes((sketch/name).read_bytes())
(build/'Arduino.h').write_text('''#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
struct String { std::string text; String(const char* value=""):text(value){} const char* c_str() const{return text.c_str();} String toString() const{return *this;} };
struct Serial {std::string text;void print(const char* value){text+=value;}void println(const char* value=""){text+=value;text+="\\n";}template<class... Args>void printf(const char* fmt,Args... args){char buf[2048];std::snprintf(buf,sizeof(buf),fmt,args...);text+=buf;}void flush(){}};
inline Serial Tab5DebugSerial;
''')
(build/'Tab5Serial.h').write_text('#include "Arduino.h"\n')
(build/'Tab5Audio.h').write_text('struct Audio {int wifiRssi(){return -50;}};inline Audio Tab5Audio;\n')
(build/'WiFi.h').write_text('''#pragma once
#include "Arduino.h"
constexpr int WL_CONNECTED=3;
struct Wifi {int status(){return 3;}String localIP(){return "127.0.0.1";}String gatewayIP(){return "127.0.0.1";}String dnsIP(){return "127.0.0.1";}};inline Wifi WiFi;
''')
(build/'esp_attr.h').write_text('#define RTC_NOINIT_ATTR\n')
(build/'sdkconfig.h').write_text('#define CONFIG_ESP_WIFI_REMOTE_ENABLED 0\n')
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
base=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-pthread']
outputs={}
map_source=(sketch/'Tab5Map.cpp').read_text()
start=map_source.index('struct Tab5MapRequestCancellation {')
end=map_source.index('\n};',start)+3
map_guard=map_source[start:end]
map_types='using Tab5NetworkCancel = bool (*)(void*);\nstruct Tab5MapConnection {Tab5CooperativeClient<Transport> plain,secure;};\n'
(build/'cooperative.cpp').write_text((here/'cooperative.cpp').read_text().replace('// MAP_CANCELLATION_GUARD',map_types+map_guard))
for name in ['cooperative','diagnostics']:
 exe=build/name
 cmd=base+['-I'+str(build),str((build if name=='cooperative' else here)/(name+'.cpp')),'-o',str(exe)]
 subprocess.run(cmd,check=True)
 result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60,env=env);print(result.stdout+result.stderr,end='');result.check_returncode();outputs[name]=result.stdout.strip()
legacy=root/'tools/network-gate-tests-0826'
cmd=base+['-I'+str(legacy),'-I'+str(sketch),str(legacy/'gate.cpp'),str(sketch/'Tab5NetworkGate.cpp'),'-o',str(build/'gate')]
subprocess.run(cmd,check=True)
result=subprocess.run([str(build/'gate')],capture_output=True,text=True,timeout=30,env=env);print(result.stdout+result.stderr,end='');result.check_returncode();outputs['gate']=result.stdout.strip()
sdk_evidence={}
if args.sdk_core:
 sdk=Path(args.sdk_core).resolve()
 def extract(text,name):
  start=text.index(name);opening=text.index('{',start);depth=1;i=opening+1
  while depth:
   if text[i]=='{':depth+=1
   elif text[i]=='}':depth-=1
   i+=1
  return text[start:i]
 http_path=sdk/'libraries/HTTPClient/src/HTTPClient.cpp'
 stream_path=sdk/'cores/esp32/Stream.cpp'
 http=http_path.read_text();stream=stream_path.read_text()
 methods=[extract(http,x) for x in ['bool HTTPClient::connected()', 'void HTTPClient::setTimeout(uint16_t timeout)', 'bool HTTPClient::connect(void)']]+[extract(stream,'int Stream::timedRead()')]
 reconnect=(here/'reconnect.cpp').read_text().replace('// SDK_FUNCTIONS','\n'.join(methods))
 (build/'reconnect.cpp').write_text(reconnect)
 subprocess.run(base+['-I'+str(build),str(build/'reconnect.cpp'),'-o',str(build/'reconnect')],check=True)
 result=subprocess.run([str(build/'reconnect')],capture_output=True,text=True,timeout=30,env=env);print(result.stdout+result.stderr,end='');result.check_returncode();outputs['sdkReconnect']=result.stdout.strip()
 sdk_evidence={'path':str(sdk),'files':{str(x.relative_to(sdk)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [http_path,stream_path]},'exactFunctions':methods}
names=['Tab5Map.cpp','Tab5CooperativeClient.h','Tab5Cooperate.h','Tab5NetDiagnostics.cpp','Tab5NetDiagnostics.h','Tab5NetworkGate.cpp','Tab5NetworkGate.h']
report={'version':'0.8.44','passed':True,'physicalDeviceTested':False,'leakSanitizer':{'enabled':False,'reason':'Runtime /proc task inspection unavailable; LeakSanitizer fatal before tests exit. ASan memory safety and UBSan enabled.'},'scope':'Unmodified production network helpers copied verbatim for host SDK adapters; gate source compiled directly. ASan/UBSan. Controlled transport, millis/RTOS and radio boundaries, not real TLS/ESP-hosted/RTOS/hardware. Concurrent RTC flag test uses host threads and actual production atomic builtins.','results':outputs,'sdkEvidence':sdk_evidence,'exactMapCancellationGuard':map_guard,'sourceHashes':{str((sketch/n).relative_to(root)):hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names},'testSourceHashes':{str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [Path(__file__),here/'cooperative.cpp',here/'diagnostics.cpp',here/'reconnect.cpp',legacy/'gate.cpp',legacy/'Arduino.h',legacy/'freertos/semphr.h']}}
out=root/'tools/verification/network-0844.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n');print(out)
