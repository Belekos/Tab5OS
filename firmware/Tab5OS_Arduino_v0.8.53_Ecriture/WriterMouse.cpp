#include "WriterMouse.h"
#include <algorithm>
#include <cstring>
namespace writing {
bool HidMouseDescriptor::parse(const uint8_t* data,size_t size){
 count_=0;ids_=false;memset(lengths_,0,sizeof(lengths_));if(!data||!size||size>4096)return false;
 struct G{uint32_t page=0,size=0,count=0,id=0;bool sign=false;}g,stack[8];unsigned sp=0,depth=0;bool collection[16]{};
 uint32_t usages[64]{},minimum=0,maximum=0;unsigned used=0;bool range=false;
 auto usage=[&](unsigned i){return used?usages[std::min(i,used-1)]:range&&maximum>=minimum?minimum+std::min<uint32_t>(i,maximum-minimum):0;};
 for(size_t at=0;at<size;){uint8_t p=data[at++];if(p==0xfe){if(size-at<2)return false;unsigned n=data[at];at+=2;if(n>size-at)return false;at+=n;continue;}
  unsigned n=p&3;if(n==3)n=4;if(n>size-at)return false;uint32_t value=0;for(unsigned i=0;i<n;++i)value|=uint32_t(data[at++])<<(i*8);
  unsigned type=(p>>2)&3,tag=p>>4;
  if(type==1){if(tag==0)g.page=value;else if(tag==1)g.sign=n&&(value&(1u<<(n*8-1)));else if(tag==7)g.size=value;else if(tag==8){if(!value||value>255)return false;g.id=value;ids_=true;}else if(tag==9)g.count=value;else if(tag==10){if(sp==8)return false;stack[sp++]=g;}else if(tag==11){if(!sp)return false;g=stack[--sp];}}
  else if(type==2){uint32_t u=n==4?value:g.page<<16|value;if(tag==0){if(used==64)return false;usages[used++]=u;}else if(tag==1){minimum=u;range=true;}else if(tag==2)maximum=u;else if(tag==10)return false;}
  else if(type==0){
   if(tag==10){if(depth==16)return false;bool mouse=depth?collection[depth-1]:false;if(value==1)mouse=usage(0)==0x10002;collection[depth++]=mouse;}
   else if(tag==12){if(!depth)return false;--depth;}
   else if(tag==8){if(!g.size||g.size>32||!g.count||g.count>1024)return false;uint32_t offset=lengths_[g.id],end=offset+g.size*g.count;if(end>4096)return false;lengths_[g.id]=end;
    if(depth&&collection[depth-1]&&!(value&1)&&(value&2))for(unsigned i=0;i<g.count;++i){uint32_t u=usage(i);uint8_t kind=0;if(u>=0x90001&&u<=0x90008)kind=uint8_t(u);else if((value&4)&&(u==0x10030||u==0x10031||u==0x10038))kind=uint8_t(u);else if((value&4)&&u==0xC0238)kind=0x39;if(!kind)continue;if(count_==32||g.size>16)return false;fields_[count_++]={uint16_t(offset+i*g.size),uint8_t(g.size),uint8_t(g.id),kind,g.sign};}
   }
   used=0;minimum=maximum=0;range=false;
  }
 }
 bool x=false,y=false;for(unsigned i=0;i<count_;++i){x|=fields_[i].usage==0x30;y|=fields_[i].usage==0x31;}
 return x&&y&&!depth&&!sp&&(!ids_||!lengths_[0]);
}
bool HidMouseDescriptor::decode(const uint8_t* data,size_t size,MouseReport& out)const{
 if(!count_||!data||!size)return false;uint8_t id=ids_?*data++:0;if(ids_)--size;if(!lengths_[id]||size*8<lengths_[id])return false;
 MouseReport r;bool found=false;for(unsigned i=0;i<count_;++i){const auto& f=fields_[i];if(f.id!=id)continue;found=true;uint32_t value=0;for(unsigned b=0;b<f.size;++b)if(data[(f.offset+b)/8]&(1u<<((f.offset+b)%8)))value|=1u<<b;
  int v=int(value);if(f.sign&&(value&(1u<<(f.size-1))))v-=1u<<f.size;
  if(f.usage==0x30)r.x=v;else if(f.usage==0x31)r.y=v;else if(f.usage==0x38)r.wheel=v;else if(f.usage==0x39)r.horizontal=v;else {r.buttonMask|=1u<<(f.usage-1);if(value)r.buttons|=1u<<(f.usage-1);}
 }if(found)out=r;return found;
}
bool HidMouseDescriptor::boot(const uint8_t* p,size_t n,MouseReport& out){if(!p||n<3)return false;out={};out.buttons=p[0]&7;out.buttonMask=7;out.x=int8_t(p[1]);out.y=int8_t(p[2]);out.wheel=n>3?int8_t(p[3]):0;return true;}
}
