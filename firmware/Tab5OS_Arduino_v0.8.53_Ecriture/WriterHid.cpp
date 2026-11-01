#include "WriterHid.h"
#include <algorithm>
#include <cstring>
namespace writing {
bool HidKeyboardDescriptor::parse(const uint8_t* data, size_t size) {
  slotCount_=0;memset(slots_,255,sizeof(slots_));for(auto& r:reports_)r=HidKeys{};
  count_ = 0; ids_ = false; memset(lengths_, 0, sizeof(lengths_));
  if (!data || !size || size > 4096) return false;
  struct Globals { uint32_t page=0, size=0, count=0, id=0; } g, stack[8];
  unsigned sp=0, depth=0;
  bool collection[16]{};
  uint32_t usages[256]{}, minimum=0, maximum=0;
  unsigned usageCount=0;
  bool hasRange=false;
  auto usageAt = [&](unsigned n) -> uint32_t {
    if (usageCount) return usages[std::min(n,usageCount-1)];
    if (hasRange && maximum >= minimum) return minimum + std::min<uint32_t>(n,maximum-minimum);
    return 0;
  };
  for (size_t p=0;p<size;) {
    uint8_t prefix=data[p++];
    if (prefix==0xfe) { if (size-p<2) return false; unsigned n=data[p]; p+=2; if(n>size-p)return false; p+=n;continue; }
    unsigned n=prefix&3; if(n==3)n=4;
    if (n>size-p) return false;
    uint32_t value=0; for(unsigned i=0;i<n;++i)value|=uint32_t(data[p++])<<(i*8);
    unsigned type=(prefix>>2)&3, tag=prefix>>4;
    if (type==1) {
      if(tag==0)g.page=value;
      else if(tag==7)g.size=value;
      else if(tag==8){if(!value||value>255)return false;g.id=value;ids_=true;}
      else if(tag==9)g.count=value;
      else if(tag==10){if(sp==8)return false;stack[sp++]=g;}
      else if(tag==11){if(!sp)return false;g=stack[--sp];}
    } else if(type==2) {
      uint32_t usage=n==4?value:(g.page<<16)|value;
      if(tag==0){if(usageCount==256)return false;usages[usageCount++]=usage;}
      else if(tag==1){minimum=usage;hasRange=true;}
      else if(tag==2)maximum=usage;
      else if(tag==10)return false; // Alternative usage sets are unsupported, not guessed.
    } else if(type==0) {
      if(tag==10){
        if(depth==16)return false;
        bool keyboard=depth?collection[depth-1]:false;
        if(value==1)keyboard=usageAt(0)==0x00010006;
        collection[depth++]=keyboard;
      } else if(tag==12){if(!depth)return false;--depth;}
      else if(tag==8) {
        if(!g.size||g.size>32||!g.count||g.count>1024)return false;
        uint32_t offset=lengths_[g.id], end=offset+g.size*g.count;
        if(end>4096)return false;
        lengths_[g.id]=end;
        if(depth&&collection[depth-1]&&!(value&1)) {
          bool variable=value&2;
          if(variable) {
            for(unsigned i=0;i<g.count;++i) {
              uint32_t u=usageAt(i);
              if((u>>16)!=7||(u&0xffff)<4||(u&0xffff)>231)continue;
              if(count_==256)return false;
              fields_[count_++]={uint16_t(offset+i*g.size),1,uint16_t(u),uint16_t(u),uint8_t(g.size),uint8_t(g.id),true};
            }
          } else if(g.page==7) {
            if(count_==256 || g.size>16)return false;
            fields_[count_++]={uint16_t(offset),uint16_t(g.count),uint16_t(hasRange?minimum:0),uint16_t(hasRange?maximum:255),uint8_t(g.size),uint8_t(g.id),false};
          }
        }
      }
      usageCount=0;minimum=maximum=0;hasRange=false;
    }
  }
  for(unsigned f=0;f<count_;++f){unsigned id=fields_[f].id;if(slots_[id]==255){if(slotCount_==16)return false;slots_[id]=slotCount_++;}}
  return count_ && !depth && !sp && (!ids_ || !lengths_[0]);
}
bool HidKeyboardDescriptor::decode(const uint8_t* data,size_t size,HidKeys& keys) const {
  if(!count_||!data||!size)return false;
  uint8_t id=0;
  if(ids_){id=*data++;--size;}
  if(!lengths_[id]||size*8<lengths_[id])return false;
  HidKeys next{}; bool keyboardReport=false,rollover=false;
  auto read=[&](unsigned bit,unsigned n){uint32_t v=0;for(unsigned b=0;b<n;++b)if(data[(bit+b)/8]&(1u<<((bit+b)%8)))v|=1u<<b;return v;};
  for(unsigned f=0;f<count_;++f){
    const auto& field=fields_[f]; if(field.id!=id)continue;
    keyboardReport=true;
    for(unsigned i=0;i<field.count;++i){
      uint32_t v=read(field.offset+i*field.size,field.size);
      if(!v)continue;
      unsigned usage=field.variable?field.usage:v;
      if(!field.variable&&(usage<4 || usage<field.usage || usage>field.maximum)){rollover=true;continue;}
      if(usage>=224&&usage<=231)next.modifiers|=1u<<(usage-224);
      else if(usage<224)next.set(usage);
    }
  }
  if(!keyboardReport)return false; // A consumer/mouse report must not release keyboard keys.
  if(rollover)next=HidKeys{};
  reports_[slots_[id]]=next;keys=HidKeys{};
  for(unsigned i=0;i<slotCount_;++i){keys.modifiers|=reports_[i].modifiers;for(unsigned b=0;b<32;++b)keys.bits[b]|=reports_[i].bits[b];}return true;
}
bool HidKeyboardDescriptor::boot(const uint8_t* data,size_t size,HidKeys& keys) {
  if(!data||size<8)return false;
  HidKeys next{};next.modifiers=data[0];
  for(unsigned i=2;i<8;++i){if(data[i]&&data[i]<4){keys=HidKeys{};return true;}if(data[i]>=4&&data[i]<224)next.set(data[i]);}
  keys=next;return true;
}
}
