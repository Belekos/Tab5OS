"""Compare the descriptor-based HID callback path across shipped versions.
Fixtures describe standard HID packets; they are not captures of the user's pad.
"""
from pathlib import Path
import argparse,subprocess,tempfile,re,json,hashlib
p=argparse.ArgumentParser();p.add_argument('work',type=Path);a=p.parse_args();work=a.work.resolve();results={}
vc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
def function(source,name):
 m=re.search(r'^(?:void|bool|uint32_t) '+name+r'\([^;{]*\)\s*\{',source,re.M);assert m,name
 depth=1
 for t in re.finditer(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\{|\}',source[m.end():],re.S):
  if t.group()=='{':depth+=1
  elif t.group()=='}':
   depth-=1
   if not depth:return source[m.start():m.end()+t.end()]
 raise ValueError(name)
with tempfile.TemporaryDirectory(prefix='hid-history-') as tmp:
 d=Path(tmp)
 for v in ['15','16','17','18','23','24']:
  sketch=work/f'Tab5OS_Ecriture_v0.8.{v}'/f'Tab5OS_Arduino_v0.8.{v}_Ecriture'
  source=(sketch/'WriterUsb.cpp').read_text(encoding='utf-8')
  iface=re.search(r'struct Interface \{.*?\};',source).group()
  pad=next(x for x in source.splitlines() if x.startswith('struct PadState{'))
  text='''
#include "WriterGamepad.h"
#include "WriterHid.h"
#include "WriterMouse.h"
#include <atomic>
#include <cstring>
#include <algorithm>
#include <cstdio>
using hid_host_device_handle_t=void*;
enum hid_host_interface_event_t {HID_HOST_INTERFACE_EVENT_INPUT_REPORT,HID_HOST_INTERFACE_EVENT_DISCONNECTED,HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR};
using QueueHandle_t=void*;constexpr int ESP_OK=0,pdTRUE=1;
static const uint8_t* data;static size_t bytes;
static int hid_host_device_get_raw_input_report_data(void*,uint8_t* out,size_t cap,size_t* n){*n=std::min(cap,bytes);memcpy(out,data,*n);return 0;}
static int xQueueSend(void*,const void*,int){return 1;}static void xQueueReset(void*){}
namespace writing {
constexpr unsigned MaxInterfaces=8;
std::atomic<bool> detachPending[MaxInterfaces],recoverPending[MaxInterfaces],overflow{false};uint32_t recoverAt[MaxInterfaces]{};
std::atomic<uint32_t> gameButtons{0},mouseCount{0};std::atomic<bool> padActivity{false},helpPressed{false};
QueueHandle_t mouseReports=nullptr,devices=nullptr;
struct DeviceEvent{void* handle;bool connected;};struct Report{HidKeys keys;bool disconnected;uint8_t slot;};
'''+iface+'\n'+pad+'''
Interface* interfaces=nullptr;PadState pads[MaxInterfaces];void publishKeyboard(Interface&,bool=false){}
'''+ '\n'.join(function(source,n) for n in ['releasePad','interfaceEvent'])+'\n}\nint main(){using namespace writing;Interface slots[MaxInterfaces];interfaces=slots;\n'
  text+='''
const uint8_t desc[]={0x05,1,0x09,5,0xa1,1,0x85,4,0x05,9,0x19,1,0x29,16,0x15,0,0x25,1,0x75,1,0x95,16,0x81,2,0x75,8,0x95,6,0x81,1,0xc0};
HidGamepadDescriptor hid;if(!hid.parse(desc,sizeof(desc)))return 3;slots[0].pad=&hid;pads[0].identity=1;
'''
  if 'deviceId' in pad:text+='pads[0].deviceId=0x60012dc8;\n'
  text+='''
const uint8_t packet[]={4,1,0,0,0,0,0,0,0};data=packet;bytes=sizeof(packet);interfaceEvent(nullptr,HID_HOST_INTERFACE_EVENT_INPUT_REPORT,&slots[0]);
unsigned got=pads[0].buttons.load(),directions=pads[0].directions.load();printf("buttons=%u expected=1, directions=%u expected=0\\n",got,directions);return got==1&&directions==0?0:1;}
'''
  (d/'test.cpp').write_text(text,encoding='utf-8')
  (d/'run.cmd').write_text('@echo off\ncall "'+vc+'" >nul\ncl /nologo /utf-8 /EHsc /std:c++17 /I"'+str(sketch)+'" test.cpp "'+str(sketch/'WriterGamepad.cpp')+'" "'+str(sketch/'WriterHid.cpp')+'" "'+str(sketch/'WriterMouse.cpp')+'" /Fe:test.exe >compile.log 2>&1\nif errorlevel 1 exit /b 99\ntest.exe\n')
  r=subprocess.run(['cmd','/d','/c',str(d/'run.cmd')],cwd=d,capture_output=True,text=True)
  if r.returncode==99:raise RuntimeError((d/'compile.log').read_text(errors='replace'))
  print('0.8.'+v,r.returncode,r.stdout.strip());results[v]=dict(exitCode=r.returncode,output=r.stdout,callbackSourceSha256=hashlib.sha256((sketch/'WriterUsb.cpp').read_bytes()).hexdigest(),decoderSourceSha256=hashlib.sha256((sketch/'WriterGamepad.cpp').read_bytes()).hexdigest())
 passed=all(results[v]['exitCode']==0 for v in ['15','16','17','18','24']) and results['23']['exitCode']==1
 report=dict(version='0.8.24',passed=passed,physicalDeviceTested=False,method='Identical standards-based 9-byte HID report with ID 4 through extracted production interfaceEvent and each version actual HID parser. USB data/queues mocked; not a physical capture.',versions=results,fixture={'descriptor':'05 01 09 05 A1 01 85 04 05 09 19 01 29 10 15 00 25 01 75 01 95 10 81 02 75 08 95 06 81 01 C0','packet':'04 01 00 00 00 00 00 00 00','model':'HID standards fixture with VID/PID 2dc8:6001; not a captured device'},sources={f:hashlib.sha256((sketch/f).read_bytes()).hexdigest() for f in ['WriterUsb.cpp','WriterGamepad.cpp','WriterGamepad.h','WriterHid.cpp','WriterHid.h','WriterMouse.cpp','WriterMouse.h']},tests={Path(__file__).name:hashlib.sha256(Path(__file__).read_bytes()).hexdigest()})
 out=work/'Tab5OS_Ecriture_v0.8.24/tools/verification/gamepad-history-0824.json';out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 raise SystemExit(0 if passed else 1)
