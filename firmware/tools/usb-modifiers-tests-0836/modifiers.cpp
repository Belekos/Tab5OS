#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "WriterHid.h"

static unsigned checks=0,failures=0;
#define CHECK(condition) do{++checks;if(!(condition)){++failures;std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#condition);}}while(0)
namespace writing {
constexpr unsigned MaxInterfaces=8;
struct Interface { HidKeys held; };
struct Report { HidKeys keys; bool disconnected; uint8_t slot=0; };
static Interface storage[MaxInterfaces];
static Interface* interfaces=storage;
static std::atomic<uint32_t> keyboardHeld[MaxInterfaces],gameButtons{0};
static std::atomic<bool> keyActive[MaxInterfaces],connected{false},overflow{false};
static std::vector<Report> queue;
static bool queueAccept=true;
static unsigned queueAttempts=0;
static int reports=1;
constexpr int pdTRUE=1;
static int xQueueSend(int,const Report* report,unsigned wait){
  CHECK(wait==0);++queueAttempts;
  if(!queueAccept)return 0;
  queue.push_back(*report);return pdTRUE;
}
#include "usb-modifiers-production.inc"
static void reset(){
  for(unsigned i=0;i<MaxInterfaces;++i){storage[i].held={};publishKeyboard(storage[i],true);}
  queue.clear();overflow=false;queueAttempts=0;queueAccept=true;
}
static void publish(unsigned slot,uint8_t bits){storage[slot].held={};storage[slot].held.modifiers=bits;publishKeyboard(storage[slot]);}
}

int main(){
 using namespace writing;
 CHECK(usbModifiers()==0);CHECK(!connected.load());
 // No usbPoll or event dequeue occurs anywhere in this harness.
 // Every HID modifier bit must be visible immediately, including modifier-only reports.
 for(unsigned slot=0;slot<MaxInterfaces;++slot){
   reset();
   for(unsigned bits=0;bits<256;++bits){
     publish(slot,uint8_t(bits));
     CHECK(usbModifiers()==bits);CHECK(connected.load());
     CHECK(queue.back().slot==slot);CHECK(queue.back().keys.modifiers==bits);
     CHECK(!queue.back().disconnected);CHECK(gameButtons.load()==0);
     const auto queued=queue.size();const auto attempts=queueAttempts;
     for(unsigned read=0;read<4;++read)CHECK(usbModifiers()==bits);
     CHECK(queue.size()==queued&&queueAttempts==attempts);
   }
   publish(slot,0);CHECK(usbModifiers()==0);CHECK(connected.load());
   publish(slot,0xff);publishKeyboard(storage[slot],true);
   CHECK(usbModifiers()==0);CHECK(!connected.load());CHECK(queue.back().disconnected);
 }
 // One keyboard releasing a modifier must not release the other keyboard's modifier.
 reset();publish(0,0x01);publish(1,0x20);CHECK(usbModifiers()==0x21);
 publish(0,0x10);CHECK(usbModifiers()==0x30);
 publishKeyboard(storage[1],true);CHECK(usbModifiers()==0x10);CHECK(connected.load());
 publish(2,0x80);publish(3,0x08);CHECK(usbModifiers()==0x98);
 publishKeyboard(storage[0],true);CHECK(usbModifiers()==0x88);
 publishKeyboard(storage[2],true);CHECK(usbModifiers()==0x08);
 publishKeyboard(storage[3],true);CHECK(usbModifiers()==0);CHECK(!connected.load());
 reset();for(unsigned i=0;i<MaxInterfaces;++i)publish(i,uint8_t(1u<<i));CHECK(usbModifiers()==0xff);
 for(unsigned i=0;i<MaxInterfaces;++i){publishKeyboard(storage[i],true);CHECK(usbModifiers()==uint8_t(0xffu<<(i+1)));}
 // A full key-event queue must not lose current physical modifier state.
 reset();queueAccept=false;publish(0,0x03);CHECK(overflow.load());CHECK(queue.empty());CHECK(usbModifiers()==0x03);
 publish(1,0x22);CHECK(usbModifiers()==0x23);
 publish(0,0);CHECK(usbModifiers()==0x22);CHECK(queue.empty());
 publishKeyboard(storage[1],true);CHECK(usbModifiers()==0);CHECK(connected.load());
 publishKeyboard(storage[0],true);CHECK(usbModifiers()==0);CHECK(!connected.load());
 // Slot reuse and stale key data on disconnect cannot retain a modifier.
 reset();publish(7,0xff);publishKeyboard(storage[7],true);CHECK(storage[7].held.modifiers==0xff);CHECK(usbModifiers()==0);
 publish(7,0);CHECK(usbModifiers()==0);CHECK(connected.load());
 // Existing game-button publication and queued key payload must be unchanged.
 reset();storage[2].held.modifiers=0x11;storage[2].held.set(79);storage[2].held.set(41);storage[2].held.set(22);
 publishKeyboard(storage[2]);CHECK(usbModifiers()==0x11);CHECK(gameButtons.load()==((1u<<0)|(1u<<8)|(1u<<11)));
 CHECK(queue.back().keys.down(79));CHECK(queue.back().keys.down(41));CHECK(queue.back().keys.down(22));
 publishKeyboard(storage[2],true);CHECK(gameButtons.load()==0);CHECK(usbModifiers()==0);
 reset();
 std::printf("{\"checks\":%u,\"failures\":%u,\"modifierPatterns\":2048,\"interfaces\":8,\"usbPollCalls\":0}\n",checks,failures);
 return failures?1:0;
}
