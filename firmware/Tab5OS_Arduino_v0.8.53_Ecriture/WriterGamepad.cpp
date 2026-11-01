#include "WriterGamepad.h"
#include <algorithm>
#include <cstring>
namespace writing {
bool decodeLegacy8BitDoGamepad(uint16_t vendor,uint16_t product,const uint8_t* data,size_t size,GamepadReport& out){
 // Protocol documented by SDL's 8BitDo driver, HandleOldStatePacket. No
 // heuristic offset shift for generic HID or for modern, ID-prefixed reports.
 if(vendor!=0x2dc8||(product!=0x6000&&product!=0x6001)||!data||size!=9)return false;
 GamepadReport r;
 // Preserve HID button numbering (including the two reserved bits).
 r.buttons=uint32_t(data[0])|(uint32_t(data[1])<<8);
 static const uint8_t hats[]={4,5,1,9,8,10,2,6};
 if(data[2]<8)r.directions=hats[data[2]];
 if(data[3]>191)r.directions|=1;else if(data[3]<64)r.directions|=2;
 if(data[4]>191)r.directions|=8;else if(data[4]<64)r.directions|=4;
 for(unsigned i=0;i<4;++i)r.axes[i]=int16_t(int(data[3+i])*257-32768);r.axisMask=15;
 r.help=(data[1]&0x10)!=0;out=r;return true;
}
bool decodeXInputGamepad(const uint8_t* data,size_t size,GamepadReport& out){
 // Byte 0 is the packet kind, byte 1 its length. Status/LED replies must not
 // release held controls. Accept endpoint padding but require all 20 bytes.
 if(!data||size<20||size>512||data[0]!=0||data[1]!=20)return false;
 GamepadReport result;
 // Normalize to the existing HID button numbering used by saved mappings:
 // A/B/X/Y, L/R, L2/R2, Select/Start, L3/R3, Guide.
 const uint8_t wireBits[]={12,13,14,15,8,9,5,4,6,7,10};
 const uint8_t buttons[]={1,2,3,4,5,6,9,10,11,12,13};
 const uint16_t digital=uint16_t(data[2])|(uint16_t(data[3])<<8);
 for(unsigned i=0;i<sizeof(wireBits);++i)if(digital&(1u<<wireBits[i]))result.buttons|=1u<<(buttons[i]-1);
 if(data[4]>30)result.buttons|=1u<<6;
 if(data[5]>30)result.buttons|=1u<<7;
 const uint8_t directions[]={4,8,2,1};
 for(unsigned i=0;i<4;++i)if(digital&(1u<<i))result.directions|=directions[i];
 auto axis=[&](unsigned offset){uint16_t raw=uint16_t(data[offset])|(uint16_t(data[offset+1])<<8);return raw&0x8000?int32_t(raw)-65536:int32_t(raw);};
 // Same half-travel dead zone as generic HID; positive XInput Y points up.
 const int32_t x=axis(6),y=axis(8);
 if(x>16383)result.directions|=1;else if(x<-16384)result.directions|=2;
 if(y>16383)result.directions|=4;else if(y<-16384)result.directions|=8;
 result.axes[0]=int16_t(x);result.axes[1]=int16_t(-1-y);result.axes[2]=int16_t(axis(10));result.axes[3]=int16_t(-1-axis(12));result.axisMask=15;
 result.help=(digital&(1u<<10))!=0;out=result;return true;
}
bool HidGamepadDescriptor::parse(const uint8_t* data,size_t size){
 count_=slotCount_=0;ids_=gamepad_=help_=false;memset(lengths_,0,sizeof(lengths_));memset(slots_,255,sizeof(slots_));for(auto& r:reports_)r={};
 if(!data||!size||size>4096)return false;
 struct G{uint32_t page=0,size=0,count=0,id=0;int32_t minimum=0,maximum=0;}g,stack[8];
 unsigned sp=0,depth=0,used=0;uint8_t scopes[16]{};uint32_t usages[64]{},minimum=0,maximum=0;bool range=false;
 auto usage=[&](unsigned i){return used?usages[std::min(i,used-1)]:range&&maximum>=minimum?minimum+std::min<uint32_t>(i,maximum-minimum):0;};
 for(size_t at=0;at<size;){uint8_t p=data[at++];if(p==0xfe){if(size-at<2)return false;unsigned n=data[at];at+=2;if(n>size-at)return false;at+=n;continue;}
  unsigned n=p&3;if(n==3)n=4;if(n>size-at)return false;uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(data[at++])<<(i*8);
  int32_t signedValue=int32_t(v);if(n&&n<4&&(v&(1u<<(n*8-1))))signedValue=int32_t(v|(~0u<<(n*8)));
  unsigned type=(p>>2)&3,tag=p>>4;
  if(type==1){if(tag==0)g.page=v;else if(tag==1)g.minimum=signedValue;else if(tag==2)g.maximum=g.minimum<0?signedValue:int32_t(v);else if(tag==7)g.size=v;else if(tag==8){if(!v||v>255)return false;g.id=v;ids_=true;}else if(tag==9)g.count=v;else if(tag==10){if(sp==8)return false;stack[sp++]=g;}else if(tag==11){if(!sp)return false;g=stack[--sp];}}
  else if(type==2){uint32_t u=n==4?v:g.page<<16|v;if(tag==0){if(used==64)return false;usages[used++]=u;}else if(tag==1){minimum=u;range=true;}else if(tag==2)maximum=u;else if(tag==10)return false;}
  else if(type==0){
   if(tag==10){if(depth==16)return false;uint8_t scope=depth?scopes[depth-1]:0;if(v==1){auto u=usage(0);scope=(u==0x10004||u==0x10005)?1:(u==0xc0001||u==0x10006||u==0x10080)?2:0;}scopes[depth++]=scope;}
   else if(tag==12){if(!depth)return false;--depth;}
   else if(tag==8){
    if(!g.size||g.size>32||!g.count||g.count>1024)return false;uint32_t offset=lengths_[g.id],end=offset+g.size*g.count;if(end>4096)return false;lengths_[g.id]=end;
    if(depth&&scopes[depth-1]&&!(v&1))for(unsigned i=0;i<g.count;++i){uint32_t u=usage(i);uint16_t kind=0;
     if(scopes[depth-1]==1){
      if(v&2){if(u>=0x90001&&u<=0x90020)kind=u&0xffff;else if(!(v&4)&&(u==0x10030||u==0x10031||u==0x10033||u==0x10034||u==0x10039||u==0x1003d||u==0x1003e||(u>=0x10090&&u<=0x10093)))kind=u&0xffff;}
      else if(g.page==9&&g.minimum>=0&&g.maximum<=32)kind=0x200;
     }else if((v&2)&&(u==0xc01a6||u==0xc01be||u==0x10087))kind=0x100;
     else if(!(v&2)&&g.page==12)kind=0x101;
     if(!kind)continue;if(count_==64||g.size>16)return false;
     fields_[count_++]={uint16_t(offset+i*g.size),kind,uint8_t(g.size),uint8_t(g.id),g.minimum,g.maximum};
     if(kind==0x100||kind==0x101)help_=true;else gamepad_=true;
    }
   }
   used=0;minimum=maximum=0;range=false;
  }
 }
 if(depth||sp||(ids_&&lengths_[0]))return false;
 for(unsigned i=0;i<count_;++i)if(slots_[fields_[i].id]==255){if(slotCount_==8)return false;slots_[fields_[i].id]=slotCount_++;}
 return count_!=0;
}
bool HidGamepadDescriptor::matchesReportSize(const uint8_t* data,size_t size)const{
 if(!data||!size||!count_)return false;unsigned id=ids_?data[0]:0;
 return slots_[id]!=255&&size==(size_t(lengths_[id])+7)/8+(ids_?1:0);
}
bool HidGamepadDescriptor::decode(const uint8_t* data,size_t size,GamepadReport& out){
 if(!count_||!data||!size)return false;uint8_t id=ids_?*data++:0;if(ids_)--size;
 if(slots_[id]==255||!lengths_[id]||size*8<lengths_[id])return false;GamepadReport r;
 for(unsigned i=0;i<count_;++i){const auto& f=fields_[i];if(f.id!=id)continue;uint32_t raw=0;for(unsigned b=0;b<f.size;++b)if(data[(f.offset+b)/8]&(1u<<((f.offset+b)%8)))raw|=1u<<b;
  int32_t v=int32_t(raw);if(f.minimum<0&&(raw&(1u<<(f.size-1))))v-=1u<<f.size;
  if(f.kind<=32){if(raw)r.buttons|=1u<<(f.kind-1);}
  else if(f.kind==0x200){if(v>=1&&v<=32&&v>=f.minimum&&v<=f.maximum)r.buttons|=1u<<(v-1);}
  else if(f.kind==0x30||f.kind==0x31||f.kind==0x33||f.kind==0x34){if(f.maximum<=f.minimum||v<f.minimum||v>f.maximum)continue;int64_t span=int64_t(f.maximum)-f.minimum;int64_t scaled=(int64_t(v)-f.minimum)*2-span; // 25% of the travel on either side of centre.
   unsigned axis=f.kind==0x30?0:f.kind==0x31?1:f.kind==0x33?2:3;r.axes[axis]=int16_t(((int64_t(v)-f.minimum)*65535/span)-32768);r.axisMask|=uint8_t(1u<<axis);
   if(axis>=2)continue;
   if(scaled>span/2)r.directions|=f.kind==0x30?1:8;else if(scaled<-span/2)r.directions|=f.kind==0x30?2:4;
  }else if(f.kind==0x39){int64_t h=int64_t(v)-f.minimum,range=int64_t(f.maximum)-f.minimum+1;if(h>=0&&h<range&&(range==4||range==8)){static const uint8_t dirs[]={4,5,1,9,8,10,2,6};r.directions|=dirs[h*(range==4?2:1)];}}
  else if(f.kind>=0x90&&f.kind<=0x93){static const uint8_t dirs[]={4,8,1,2};if(raw)r.directions|=dirs[f.kind-0x90];}
  else if(f.kind==0x3d&&raw)r.directions|=128;else if(f.kind==0x3e&&raw)r.directions|=64;
  else if(f.kind==0x100)r.help|=raw!=0;else if(f.kind==0x101)r.help|=raw==0x1a6||raw==0x1be;
 }
 reports_[slots_[id]]=r;out={};for(unsigned i=0;i<slotCount_;++i){out.buttons|=reports_[i].buttons;out.directions|=reports_[i].directions;out.help|=reports_[i].help;for(unsigned a=0;a<4;++a)if(reports_[i].axisMask&(1u<<a)){out.axes[a]=reports_[i].axes[a];out.axisMask|=uint8_t(1u<<a);}}return true;
}
}
