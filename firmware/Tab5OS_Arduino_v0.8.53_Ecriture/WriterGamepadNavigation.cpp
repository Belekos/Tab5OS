#include "WriterGamepadNavigation.h"
#include "WriterUsb.h"
#include <atomic>
#include <cerrno>
#include <cstring>
namespace writing {
bool GamepadNavigation::poll(uint32_t buttons,uint32_t now,bool enabled,KeyEvent& event){
 const uint32_t supported=0x78ff;
 buttons&=supported;
 if(!enabled||enabled!=enabled_){held_=buttons;pending_=0;repeatAt_=now+400;enabled_=enabled;return false;}
 const uint32_t pressed=buttons&~held_;held_=buttons;pending_=(pending_|pressed)&buttons;
 if(pressed&15)repeatAt_=now+400;
 if(!pending_&&(buttons&15)&&int32_t(now-repeatAt_)>=0){pending_=buttons&15;repeatAt_=now+110;}
 if(!pending_)return false;
 unsigned bit=0;while(!(pending_&(1u<<bit)))++bit;
 pending_&=~(1u<<bit);event={};
 switch(bit){
  case 0:event.key=Key::Right;break;case 1:event.key=Key::Left;break;
  case 2:event.key=Key::Up;break;case 3:event.key=Key::Down;break;
  case 4:event.key=Key::Enter;break;case 5:event.key=Key::Escape;break;
  case 6:case 11:event.key=Key::ContextMenu;break;
  case 7:event.key=Key::F1;break;case 12:event.key=Key::F6;break;
  case 13:event.key=Key::PageUp;break;case 14:event.key=Key::PageDown;break;
  default:return false;
 }
 return true;
}
static std::atomic<bool> pythonCaptured{false};
bool gamepadPythonCaptured(){return pythonCaptured.load();}
void gamepadPythonReset(){pythonCaptured=false;}
int gamepadPythonCall(int operation,int value,void* output,size_t capacity){
 if(operation==1){pythonCaptured=value!=0;return 0;}
 if(operation!=0)return -EINVAL;
 if(!output||capacity<64)return -ENOSPC;
 GamepadInfo info;bool present=usbGamepadInfo(info);
 auto* p=static_cast<uint8_t*>(output);memset(p,0,64);
 auto put=[&](unsigned at,uint32_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i)p[at+i]=uint8_t(value>>(8*i));};
 put(0,info.identity,4);put(4,usbGamepadButtons(),4);put(8,info.buttons,4);
 put(12,info.reports,4);put(16,info.rejected,4);put(20,info.errors,4);put(24,info.restarts,4);
 put(28,info.vendor,2);put(30,info.product,2);
 for(unsigned i=0;i<4;++i)put(32+i*2,uint16_t(info.axes[i]),2);
 p[40]=info.directions;p[41]=info.axisMask;p[42]=info.xinput;p[43]=present;
 p[44]=info.rawSize;p[45]=info.interfaceNumber;memcpy(p+48,info.raw,16);
 return 64;
}
}
