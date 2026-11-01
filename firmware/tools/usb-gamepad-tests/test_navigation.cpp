#include "WriterGamepadNavigation.h"
#include "WriterUsb.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cerrno>
namespace writing {
static GamepadInfo live;static uint32_t held;
bool usbGamepadInfo(GamepadInfo& o){o=live;return bool(live.identity);}
uint32_t usbGamepadButtons(){return held;}
}
int main(){
 using namespace writing;using K=Key;GamepadNavigation n;KeyEvent event;
 const unsigned bits[]={0,1,2,3,4,5,6,7,11,12,13,14};
 const K keys[]={K::Right,K::Left,K::Up,K::Down,K::Enter,K::Escape,K::ContextMenu,K::F1,K::ContextMenu,K::F6,K::PageUp,K::PageDown};
 for(unsigned i=0;i<12;++i){assert(!n.poll(0,0,true,event));assert(n.poll(1u<<bits[i],1,true,event));assert(event.key==keys[i]);assert(!n.poll(1u<<bits[i],2,true,event));}
 assert(!n.poll(0,100,true,event));assert(n.poll(4,101,true,event));
 assert(!n.poll(4,500,true,event));assert(n.poll(4,501,true,event));assert(event.key==K::Up);
 assert(!n.poll(4,610,true,event));assert(n.poll(4,611,true,event));
 // A held validation button never repeats, even when changing context.
 assert(n.poll(16,700,true,event));assert(!n.poll(16,100000,true,event));
 assert(!n.poll(16,100001,false,event));assert(!n.poll(16,100002,true,event));
 assert(!n.poll(16,100003,true,event));assert(!n.poll(0,100004,true,event));
 assert(n.poll(16,100005,true,event)&&event.key==K::Enter);
 // A rapid release removes pending simultaneous presses; no phantom command.
 assert(n.poll(12,100006,true,event));assert(!n.poll(0,100007,true,event));
 GamepadNavigation wrap;assert(wrap.poll(1,0xffffff00,true,event));
 assert(!wrap.poll(1,0x8f,true,event));assert(wrap.poll(1,0x90,true,event));
 uint8_t out[64];memset(out,0xff,sizeof(out));
 assert(gamepadPythonCall(0,0,out,63)==-ENOSPC);
 assert(gamepadPythonCall(9,0,out,64)==-EINVAL);
 assert(gamepadPythonCall(0,0,out,64)==64&&out[43]==0);
 live.identity=0x12345678;live.vendor=0x2dc8;live.product=0x6001;
 live.buttons=0x80000000;live.reports=17;live.rawSize=9;live.raw[0]=4;
 live.axes[0]=-32768;live.axes[1]=32767;live.axisMask=3;held=16;
 assert(gamepadPythonCall(0,0,out,64)==64);
 assert(out[0]==0x78&&out[3]==0x12&&out[4]==16&&out[11]==0x80);
 assert(out[28]==0xc8&&out[29]==0x2d&&out[30]==1&&out[31]==0x60);
 assert(out[32]==0&&out[33]==0x80&&out[34]==0xff&&out[35]==0x7f);
 assert(out[41]==3&&out[43]==1&&out[44]==9&&out[48]==4);
 // Multiple UI/game/VM readers retain held state; capture is independent.
 for(unsigned i=0;i<4000;++i){assert(gamepadPythonCall(0,0,out,64)==64);assert(out[4]==16&&held==16);}
 assert(gamepadPythonCall(1,1,nullptr,0)==0&&gamepadPythonCaptured());
 gamepadPythonReset();assert(!gamepadPythonCaptured());
 std::puts("PASS: OS navigation, 12 controls, no validation repeat, capture/context changes, timer wrap, 64-byte Python bridge, 4000 non-consuming snapshots.");
}
