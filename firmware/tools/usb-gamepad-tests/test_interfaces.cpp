#include "src/usb_hid/input_interface.h"
#include <vector>
#include <cassert>
#include <iostream>
using Bytes=std::vector<uint8_t>;
static int checks=0;
#define CHECK(x) do {++checks; assert(x);} while(0)
static void append(Bytes& b, std::initializer_list<uint8_t> v){b.insert(b.end(),v);}
static Bytes configuration(){return {9,2,0,0,0,1,0,0x80,50};}
static void finish(Bytes& b){b[2]=uint8_t(b.size());b[3]=uint8_t(b.size()>>8);}
static void hid(Bytes& b,uint8_t number,uint8_t alternate=0,uint8_t protocol=0){
 append(b,{9,4,number,alternate,1,3,uint8_t(protocol?1:0),protocol,0});
 append(b,{9,0x21,0x11,1,0,1,0x22,50,0});append(b,{7,5,uint8_t(0x81+number),3,64,0,4});
}
static void xbox(Bytes& b,uint8_t number){
 append(b,{9,4,number,0,2,0xff,0x5d,1,0});
 append(b,{7,5,uint8_t(0x81+number),3,32,0,4});append(b,{7,5,uint8_t(1+number),3,32,0,4});
}
int main(){
 tab5_usb_input_interface_t out{};
 auto b=configuration();hid(b,0);finish(b);size_t cursor=0;
 CHECK(tab5_usb_next_input_interface(b.data(),b.size(),0x2dc8,&cursor,&out));
 CHECK(!out.xinput&&out.interface_desc[2]==0&&out.endpoint_desc[2]==0x81);
 CHECK(!tab5_usb_next_input_interface(b.data(),b.size(),0x2dc8,&cursor,&out));
 // Both protocols on a composite descriptor: no VID/PID override of HID.
 b=configuration();hid(b,0);xbox(b,1);hid(b,2,0,1);hid(b,3,0,2);
 append(b,{9,4,4,0,2,8,6,0x50,0});append(b,{7,5,0x85,2,64,0,0});append(b,{7,5,5,2,64,0,0});finish(b);
 cursor=0;
 for(int i=0;i<4;++i){CHECK(tab5_usb_next_input_interface(b.data(),b.size(),0x2dc8,&cursor,&out));CHECK(out.interface_desc[2]==i);CHECK(out.xinput==(i==1));}
 CHECK(!tab5_usb_next_input_interface(b.data(),b.size(),0x2dc8,&cursor,&out));
 // Do not borrow the next HID descriptor for an incomplete interface.
 b=configuration();append(b,{9,4,0,0,1,3,0,0,0});append(b,{7,5,0x81,3,64,0,4});hid(b,1);finish(b);cursor=0;
 CHECK(tab5_usb_next_input_interface(b.data(),b.size(),0x2dc8,&cursor,&out));CHECK(out.interface_desc[2]==1);
 // Ignore alternate settings. The driver claims alternate zero only.
 b=configuration();hid(b,0,1);hid(b,1);finish(b);cursor=0;
 CHECK(tab5_usb_next_input_interface(b.data(),b.size(),0x2dc8,&cursor,&out));CHECK(out.interface_desc[2]==1);
 // Select a valid game endpoint after a short status endpoint, rather than
 // abandoning an otherwise valid XInput interface after the first IN endpoint.
 b=configuration();append(b,{9,4,0,0,2,0xff,0x5d,1,0});append(b,{7,5,0x81,3,8,0,4});append(b,{7,5,0x82,3,32,0,4});finish(b);cursor=0;
 CHECK(tab5_usb_next_input_interface(b.data(),b.size(),0x2dc8,&cursor,&out));CHECK(out.xinput&&out.endpoint_desc[2]==0x82);
 cursor=0;CHECK(!tab5_usb_next_input_interface(b.data(),b.size(),0x1234,&cursor,&out));
 // Exhaustively reject every truncated prefix of a real accepted fixture.
 b=configuration();hid(b,0);finish(b);
 for(size_t n=0;n<b.size();++n){cursor=0;CHECK(!tab5_usb_next_input_interface(b.data(),n,0x2dc8,&cursor,&out));}
 for(size_t pos: {size_t(0),size_t(9),size_t(18),size_t(27)})for(uint8_t size:{uint8_t(0),uint8_t(1),uint8_t(255)}){
  auto damaged=b;damaged[pos]=size;cursor=0;CHECK(!tab5_usb_next_input_interface(damaged.data(),damaged.size(),0x2dc8,&cursor,&out));
 }
 // Repeated mode switches always use the matching descriptor, with no retained state.
 for(int i=0;i<2000;++i){b=configuration();if(i&1)xbox(b,0);else hid(b,0);finish(b);cursor=0;
  CHECK(tab5_usb_next_input_interface(b.data(),b.size(),0x2dc8,&cursor,&out));CHECK(out.xinput==bool(i&1));
 }
 std::cout<<"PASS: "<<checks<<" bounded composite enumeration assertions\n";
}
