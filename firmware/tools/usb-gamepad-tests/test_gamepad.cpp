#include "WriterGamepad.h"
#include "WriterHid.h"
#include "src/usb_hid/gamepad_transport.h"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
using writing::GamepadReport;

static std::array<uint8_t,20> packet(){std::array<uint8_t,20> p{};p[1]=20;return p;}
static GamepadReport decode(const std::array<uint8_t,20>& p){GamepadReport r;assert(writing::decodeXInputGamepad(p.data(),p.size(),r));return r;}
static void axis(std::array<uint8_t,20>& p,unsigned at,int value){p[at]=uint8_t(value);p[at+1]=uint8_t(uint16_t(value)>>8);}
int main(){
 // The real enumeration policy must not take over storage, SDR, keyboards,
 // a wireless Xbox receiver, Xbox One, or an unrelated vendor's interface.
 assert(tab5_usb_xinput_interface(0x2dc8,0xff,0x5d,1,0));
 assert(tab5_usb_xinput_interface(0x045e,0xff,0x5d,1,0));
 for(auto vendor:{0x040b,0x0bda,0x1234})assert(!tab5_usb_xinput_interface(uint16_t(vendor),0xff,0x5d,1,0));
 assert(!tab5_usb_xinput_interface(0x2dc8,0xff,0x5d,0x81,0));
 assert(!tab5_usb_xinput_interface(0x2dc8,0xff,0x47,0xd0,0));
 assert(!tab5_usb_xinput_interface(0x2dc8,0xff,0x5d,1,1));
 assert(!tab5_usb_xinput_interface(0x2dc8,3,1,1,0));
 assert(!tab5_usb_xinput_interface(0x2dc8,8,6,0x50,0));
 assert(tab5_usb_interrupt_input(0x81,3,32,true));
 assert(tab5_usb_interrupt_input(0x81,3,8,false));
 assert(!tab5_usb_interrupt_input(0x81,3,19,true));
 assert(!tab5_usb_interrupt_input(0x81,2,32,true));
 assert(!tab5_usb_interrupt_input(0x01,3,32,true));
 assert(!tab5_usb_interrupt_input(0x80,3,32,true));
 assert(!tab5_usb_interrupt_input(0x81,3,1024,true));

 auto p=packet();auto r=decode(p);assert(!r.buttons&&!r.directions&&!r.help);
 // Protocol fixtures encode wire packets, not reports captured on a Tab5.
 // Up-right + A + Start: 00 14 19 10 00 00 ...
 p[2]=0x19;p[3]=0x10;r=decode(p);assert(r.directions==5);assert(r.buttons==((1u<<0)|(1u<<9)));
 const unsigned wire[]={12,13,14,15,8,9,5,4,6,7,10};
 const unsigned button[]={1,2,3,4,5,6,9,10,11,12,13};
 for(unsigned i=0;i<11;++i){p=packet();p[2+wire[i]/8]=uint8_t(1u<<(wire[i]%8));r=decode(p);assert(r.buttons==(1u<<(button[i]-1)));assert(r.help==(wire[i]==10));}
 const unsigned directions[]={4,8,2,1};
 for(unsigned i=0;i<4;++i){p=packet();p[2]=uint8_t(1u<<i);assert(decode(p).directions==directions[i]);}
 for(int x:{-32768,-16385,-16384,-1,0,16383,16384,32767})for(int y:{-32768,-16385,-16384,-1,0,16383,16384,32767}){
  p=packet();axis(p,6,x);axis(p,8,y);r=decode(p);assert(r.directions==((x>16383?1:x<-16384?2:0)|(y>16383?4:y<-16384?8:0)));assert(!r.buttons);
 }
 p=packet();p[4]=30;p[5]=30;assert(!decode(p).buttons);p[4]=31;p[5]=255;assert(decode(p).buttons==0xc0);
 p=packet();p[2]=0xff;p[3]=0xff;p[4]=255;p[5]=255;r=decode(p);assert(r.buttons==0x1fff&&r.directions==15&&r.help);
 p=packet();r=decode(p);assert(!r.buttons&&!r.directions&&!r.help); // release
 // Reject truncated, unrelated and malformed packets without changing output.
 r={0x55,0xaa,true};p=packet();for(size_t n=0;n<20;++n){assert(!writing::decodeXInputGamepad(p.data(),n,r));assert(r.buttons==0x55&&r.directions==0xaa&&r.help);}
 assert(!writing::decodeXInputGamepad(nullptr,20,r));
 p[0]=1;assert(!writing::decodeXInputGamepad(p.data(),20,r));p=packet();p[1]=19;assert(!writing::decodeXInputGamepad(p.data(),20,r));
 std::array<uint8_t,32> padded{};padded[1]=20;padded[3]=0x20;assert(writing::decodeXInputGamepad(padded.data(),32,r)&&r.buttons==2);
 assert(!writing::decodeXInputGamepad(padded.data(),513,r));

 // Generic HID remains descriptor-based. ID 1 has 16 buttons, an 8-way hat
 // (8 means neutral) and X/Y. ID 2 independently carries button 17.
 const uint8_t descriptor[]={
  0x05,1,0x09,5,0xa1,1,0x85,1,
  0x05,9,0x19,1,0x29,16,0x15,0,0x25,1,0x75,1,0x95,16,0x81,2,
  0x05,1,0x09,0x39,0x15,0,0x25,7,0x75,4,0x95,1,0x81,0x42,
  0x75,4,0x95,1,0x81,1,
  0x09,0x30,0x09,0x31,0x15,0,0x26,0xff,0,0x75,8,0x95,2,0x81,2,
  0x85,2,0x05,9,0x09,17,0x15,0,0x25,1,0x75,1,0x95,1,0x81,2,
  0x75,7,0x95,1,0x81,1,0xc0};
 writing::HidGamepadDescriptor hid;assert(hid.parse(descriptor,sizeof descriptor)&&hid.gamepad());
 uint8_t report[]={1,1,0,1,128,128};assert(hid.decode(report,sizeof report,r));assert(r.buttons==1&&r.directions==5);
 const uint8_t second[]={2,1};assert(hid.decode(second,2,r));assert(r.buttons==0x10001&&r.directions==5);
 report[1]=0;report[3]=8;assert(hid.decode(report,sizeof report,r));assert(r.buttons==0x10000&&!r.directions);
 const uint8_t secondRelease[]={2,0};assert(hid.decode(secondRelease,2,r));assert(!r.buttons&&!r.directions);
 report[4]=0;report[5]=255;assert(hid.decode(report,sizeof report,r));assert(r.directions==10);
 assert(!hid.decode(report,sizeof report-1,r));
 const uint8_t unknown[]={99,0,0,0,0,0};assert(!hid.decode(unknown,sizeof unknown,r));
 writing::HidKeyboardDescriptor keyboard;assert(!keyboard.parse(descriptor,sizeof descriptor));
 std::cout<<"PASS: wired XInput classification, 11 buttons, D-pad, 64 axis cases, triggers, release, lengths, unrelated packets, HID multi-report regression.\n";
}
