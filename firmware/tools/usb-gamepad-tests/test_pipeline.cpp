#include "WriterGamepad.h"
#include "WriterHid.h"
#include "WriterMouse.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <array>
static unsigned checks;
#undef assert
#define assert(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);std::abort();}}while(0)
using hid_host_device_handle_t=void*;
enum hid_host_interface_event_t{HID_HOST_INTERFACE_EVENT_INPUT_REPORT,HID_HOST_INTERFACE_EVENT_DISCONNECTED,HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR};
constexpr int ESP_OK=0,pdTRUE=1;
using QueueHandle_t=void*;
static std::array<uint8_t,512> incoming{};static size_t incomingSize;
static int hid_host_device_get_raw_input_report_data(void*,uint8_t* dest,size_t capacity,size_t* n){*n=std::min(capacity,incomingSize);std::memcpy(dest,incoming.data(),*n);return ESP_OK;}
static bool mouseQueueFull=false;static unsigned mouseQueueResets=0,mouseQueueWrites=0;
static writing::MouseReport latestMouse{};
static int xQueueSend(void*,const void* data,int){if(mouseQueueFull)return 0;latestMouse=*static_cast<const writing::MouseReport*>(data);++mouseQueueWrites;return pdTRUE;}
static void xQueueReset(void*){mouseQueueFull=false;++mouseQueueResets;}
static uint32_t fakeNow;static unsigned recoveryStops,recoveryStarts;static bool recoveryFail;
static uint32_t millis(){return fakeNow;}
static int hid_host_device_stop(void*){++recoveryStops;return ESP_OK;}
static int hid_host_device_start(void*){++recoveryStarts;return recoveryFail?1:ESP_OK;}
namespace writing {
#include "pipeline_production.inc"
}
int main(){
 using namespace writing;
 Interface slots[MaxInterfaces];interfaces=slots;for(unsigned i=0;i<MaxInterfaces;++i)slots[i].handle=reinterpret_cast<void*>(uintptr_t(i+1));
 auto deliver=[&](unsigned slot,const uint8_t* bytes,size_t n){incomingSize=n;std::memcpy(incoming.data(),bytes,n);interfaceEvent(slots[slot].handle,HID_HOST_INTERFACE_EVENT_INPUT_REPORT,&slots[slot]);};
 auto xinput=std::array<uint8_t,20>{};xinput[1]=20;
 slots[0].xinput=true;pads[0].identity=1;pads[0].deviceId=0x60012dc8;pads[0].xinput=true;
 // The application callback, atomics and public read APIs all execute without
 // opening the emulator or calling its F1/updatePad function.
 for(unsigned round=0;round<2000;++round){
  xinput[2]=0x19;xinput[3]=0x10;deliver(0,xinput.data(),xinput.size());
  GamepadInfo info;assert(usbGamepadInfo(info));assert(info.xinput&&info.vendor==0x2dc8&&info.product==0x6001);
  assert(info.buttons==0x201&&info.directions==5);assert(usbGameButtons()==(5|16|128));
  xinput[2]=xinput[3]=0;deliver(0,xinput.data(),xinput.size());assert(usbGameButtons()==0);
 }
 xinput[3]=0x10;deliver(0,xinput.data(),19);GamepadInfo info;assert(usbGamepadInfo(info));assert(info.rejected==1&&info.reports==4000);
 // Multi-report generic HID: buttons 1..16, hat/X/Y and a separate button 17.
 const uint8_t desc[]={0x05,1,0x09,5,0xa1,1,0x85,3,
  0x05,9,0x19,1,0x29,16,0x15,0,0x25,1,0x75,1,0x95,16,0x81,2,
  0x05,1,0x09,0x39,0x15,0,0x25,7,0x75,4,0x95,1,0x81,0x42,0x75,4,0x95,1,0x81,1,
  0x09,0x30,0x09,0x31,0x15,0,0x26,0xff,0,0x75,8,0x95,2,0x81,2,
  0x85,4,0x05,9,0x09,17,0x15,0,0x25,1,0x75,1,0x95,1,0x81,2,0x75,7,0x95,1,0x81,1,0xc0};
 HidGamepadDescriptor hid;assert(hid.parse(desc,sizeof(desc)));slots[1].pad=&hid;pads[1].identity=2;pads[1].deviceId=0x12345678;
 const uint8_t press[]={3,2,0,2,128,128},release[]={3,0,0,8,128,128},extra[]={4,1},extraUp[]={4,0};
 for(unsigned round=0;round<2000;++round){
  deliver(1,press,sizeof(press));assert(usbGameButtons()==(32|1));
  xinput[3]=0x10;deliver(0,xinput.data(),20);assert(usbGameButtons()==(16|32|1));
  deliver(1,extra,sizeof(extra));assert(pads[1].buttons==0x10002);
  deliver(1,release,sizeof(release));assert(pads[1].buttons==0x10000&&usbGameButtons()==16);
  deliver(1,extraUp,sizeof(extraUp));xinput[3]=0;deliver(0,xinput.data(),20);assert(usbGameButtons()==0);
 }
 // Targeted legacy HID fallback. It must never reinterpret another VID/PID,
 // nor a modern report, as a nine-byte SN30 packet.
 const uint8_t legacy[]={1,0x0c,1,127,127,127,127,0,0};
 slots[2].pad=&hid;pads[2].identity=3;pads[2].deviceId=0x60012dc8;
 pads[2].mapping=0x0c0b0201;pads[2].extended=0x08070504;
 deliver(2,legacy,sizeof(legacy));assert(pads[2].buttons==0x0c01);assert(usbGameButtons()==(5|16|64|128));
 GamepadReport untouched{0x55,0xaa,true};
 assert(!decodeLegacy8BitDoGamepad(0x1234,0x6001,legacy,9,untouched));assert(untouched.buttons==0x55);
 assert(!decodeLegacy8BitDoGamepad(0x2dc8,0x9999,legacy,9,untouched));
 for(size_t n=0;n<9;++n)assert(!decodeLegacy8BitDoGamepad(0x2dc8,0x6001,legacy,n,untouched));
 assert(!decodeLegacy8BitDoGamepad(0x2dc8,0x6001,legacy,10,untouched));
 for(unsigned bit=0;bit<15;++bit){
  uint8_t bytes[]={0,0,8,127,127,127,127,0,0};bytes[bit/8]=uint8_t(1u<<(bit%8));
  deliver(2,bytes,9);assert(pads[2].buttons==(1u<<bit));
 }
 // Regression in .23: any nine-byte 8BitDo packet was decoded as legacy,
 // including correctly described ID-prefixed modern reports. Preserve HID.
 const uint8_t modernDesc[]={0x05,1,0x09,5,0xa1,1,0x85,4,
  0x05,9,0x19,1,0x29,16,0x15,0,0x25,1,0x75,1,0x95,16,0x81,2,
  0x75,8,0x95,6,0x81,1,0xc0};
 HidGamepadDescriptor modern;assert(modern.parse(modernDesc,sizeof(modernDesc)));
 slots[2].pad=&modern;
 const uint8_t modernPacket[]={4,1,0,0,0,0,0,0,0};
 deliver(2,modernPacket,9);assert(pads[2].buttons==1&&pads[2].directions==0);
 assert(pads[2].rawSize==9&&pads[2].raw[0]==4&&pads[2].raw[1]==1);
 interfaceEvent(nullptr,HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR,&slots[2]);assert(usbGameButtons()==0);
 assert(recoverPending[2]&&pads[2].errors==1);recoveryFail=true;
 serviceRecovery();assert(recoveryStops==1&&recoveryStarts==1&&recoverPending[2]);
 fakeNow=999;serviceRecovery();assert(recoveryStarts==1);
 fakeNow=1000;recoveryFail=false;serviceRecovery();
 assert(recoveryStarts==2&&!recoverPending[2]&&pads[2].restarts==1);
 // No recovery after detach, no stale controls retained and no polling UI needed.
 recoverPending[2]=true;detachPending[2]=true;serviceRecovery();assert(recoveryStarts==2);
 interfaceEvent(nullptr,HID_HOST_INTERFACE_EVENT_DISCONNECTED,&slots[0]);assert(pads[0].identity==0&&pads[0].reports==0);
 interfaceEvent(nullptr,HID_HOST_INTERFACE_EVENT_DISCONNECTED,&slots[1]);
 interfaceEvent(nullptr,HID_HOST_INTERFACE_EVENT_DISCONNECTED,&slots[2]);
 assert(!usbGamepadInfo(info)&&!usbGameButtons());
 // A lost release leaves LVGL holding a button if another mouse remains
 // connected. The latest aggregate state must replace a full queue on both
 // transfer errors and disconnect, preserving the second mouse's buttons.
 slots[3].isMouse=slots[3].activeMouse=true;slots[3].buttons=1;
 slots[4].isMouse=slots[4].activeMouse=true;slots[4].buttons=2;mouseCount=2;
 mouseQueueFull=true;unsigned writes=mouseQueueWrites,resets=mouseQueueResets;
 interfaceEvent(nullptr,HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR,&slots[3]);
 assert(mouseQueueWrites==writes+1&&mouseQueueResets==resets+1);
 assert(latestMouse.buttons==2&&mouseCount==2&&slots[3].buttons==0);
 slots[3].buttons=1;mouseQueueFull=true;writes=mouseQueueWrites;resets=mouseQueueResets;
 interfaceEvent(nullptr,HID_HOST_INTERFACE_EVENT_DISCONNECTED,&slots[3]);
 assert(mouseQueueWrites==writes+1&&mouseQueueResets==resets+1);
 assert(latestMouse.buttons==2&&mouseCount==1&&!slots[3].activeMouse&&detachPending[3]);
 std::printf("PASS: %u production callback/API assertions; HID and XInput without F1, 4000 press/release cycles, targeted legacy packets, disconnect, mouse release with a full queue.\n",checks);
}
