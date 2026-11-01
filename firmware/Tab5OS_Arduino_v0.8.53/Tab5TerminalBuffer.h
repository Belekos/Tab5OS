#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace Tab5Terminal {
template<size_t Capacity> class OutputRing {
 char bytes_[Capacity]{};size_t first_=0,size_=0;
 public:
 void clear(){first_=size_=0;}
 size_t size()const{return size_;}
 void append(const char* p,size_t n){
  // Clear affects only what precedes its last occurrence in this write.
  for(size_t i=n;i;--i)if(p[i-1]=='\f'){clear();p+=i;n-=i;break;}
  if(n>=Capacity){p+=n-(Capacity-1);n=Capacity-1;clear();}
  if(size_+n>=Capacity){size_t drop=size_+n-Capacity+1;first_=(first_+drop)%Capacity;size_-=drop;}
  const size_t at=(first_+size_)%Capacity,part=std::min(n,Capacity-at);memcpy(bytes_+at,p,part);memcpy(bytes_,p+part,n-part);size_+=n;
  // A wrapped UTF-8 character is discarded once, without shifting the ring.
  while(size_&&(uint8_t(bytes_[first_])&0xc0)==0x80){first_=(first_+1)%Capacity;--size_;}
 }
 size_t copy(char* out,size_t capacity)const{if(!capacity)return 0;size_t n=std::min(size_,capacity-1),first=first_;if(n<size_){first=(first+size_-n)%Capacity;while(n&&(uint8_t(bytes_[first])&0xc0)==0x80){first=(first+1)%Capacity;--n;}}
  size_t part=std::min(n,Capacity-first);memcpy(out,bytes_+first,part);memcpy(out+part,bytes_,n-part);out[n]=0;return n;
 }
};
// Cancellation is checked each time; cooperative yielding is time-based.
struct YieldCadence {
 uint32_t last=0;
 bool due(uint32_t now){if(uint32_t(now-last)<5)return false;last=now;return true;}
};
}
