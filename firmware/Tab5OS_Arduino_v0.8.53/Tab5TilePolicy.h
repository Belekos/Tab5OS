#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace tab5tile {
constexpr unsigned MaxAttempts = 4;
constexpr size_t MaxBytes = 512 * 1024;
inline bool retryable(int code) {
  return (code < 0 && code != -102 && code != -103 && code != -107) ||
         code == 408 || code == 429 || (code >= 500 && code <= 599);
}
inline unsigned backoff(unsigned attempt) { return attempt == 1 ? 400 : attempt == 2 ? 1200 : 2500; }
template<class Download,class Current,class Wait,class Log>
bool downloadWithRetries(int& code,uint8_t& attempts,Download download,Current current,Wait wait,Log log){
 for(unsigned i=1;i<=MaxAttempts;++i){
  if(!current()){code=-107;return false;}attempts=i;
  if(download())return true;
  if(!retryable(code)||i==MaxAttempts)return false;
  log(i,code);wait(backoff(i));
 }return false;
}
inline uint32_t be32(const uint8_t* p) { return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]; }
inline uint32_t crc(const uint8_t* p, size_t n) {
  static constexpr uint32_t table[16] = {0,0x1db71064,0x3b6e20c8,0x26d930ac,0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,0xedb88320,0xf00f9344,0xd6d6a3e8,0xcb61b38c,0x9b64c2b0,0x86d3d2d4,0xa00ae278,0xbdbdf21c};
  uint32_t value=~0u;while(n--){value^=*p++;value=(value>>4)^table[value&15];value=(value>>4)^table[value&15];}return ~value;
}
// Reject truncated/corrupt tiles before passing them to the shared image decoder.
// XYZ providers configured in this OS use 256 x 256 pixel PNG tiles.
inline bool validPng(const uint8_t* p, size_t n) {
  const uint8_t sig[]={137,80,78,71,13,10,26,10};
  if(!p||n<57||n>MaxBytes||memcmp(p,sig,8))return false;
  bool ihdr=false,idat=false,idatEnded=false,palette=false;size_t at=8;
  while(n-at>=12){uint32_t len=be32(p+at);if(len>n-at-12)return false;
    const auto* type=p+at+4;const auto* data=p+at+8;
    if(crc(type,size_t(len)+4)!=be32(data+len))return false;
    if(!ihdr){
      if(memcmp(type,"IHDR",4)||len!=13||be32(data)!=256||be32(data+4)!=256||data[10]||data[11]||data[12]>1)return false;
      const unsigned depth=data[8],color=data[9];
      if(!((color==0&&(depth==1||depth==2||depth==4||depth==8||depth==16))||(color==3&&(depth==1||depth==2||depth==4||depth==8))||((color==2||color==4||color==6)&&(depth==8||depth==16))))return false;
      ihdr=true;
    }else if(!memcmp(type,"IHDR",4))return false;
    if(!memcmp(type,"PLTE",4)){if(palette||idat||!len||len>768||len%3)return false;palette=true;}
    if(!memcmp(type,"IDAT",4)){if(idatEnded)return false;if(len)idat=true;}
    else if(idat)idatEnded=true;
    if(!memcmp(type,"tRNS",4)&&idat)return false;
    if(type[0]>='A'&&type[0]<='Z'&&memcmp(type,"IHDR",4)&&memcmp(type,"PLTE",4)&&memcmp(type,"IDAT",4)&&memcmp(type,"IEND",4))return false;
    at+=size_t(len)+12;
    if(!memcmp(type,"IEND",4))return len==0&&idat&&at==n;
  }return false;
}
}
