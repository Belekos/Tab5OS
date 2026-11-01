#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace Tab5Ico {
inline uint16_t u16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
struct Frame {const uint8_t* pixels=nullptr;const uint8_t* mask=nullptr;unsigned w=0,h=0,bpp=0,stride=0,maskStride=0;};
inline bool inspect(const uint8_t* bytes,size_t size,Frame& best){
  best={};if(!bytes||size<22||size>32768||u16(bytes)||u16(bytes+2)!=1)return false;
  unsigned n=u16(bytes+4);if(!n||n>16||size<6+size_t(n)*16)return false;
  unsigned score=UINT32_MAX;
  for(unsigned i=0;i<n;++i){
    const uint8_t* entry=bytes+6+i*16;unsigned w=entry[0],h=entry[1];
    size_t length=u32(entry+8),offset=u32(entry+12);
    if(!w||!h||w>64||h>64||offset<6+size_t(n)*16||offset>size||length>size-offset||length<40)continue;
    const uint8_t* dib=bytes+offset;unsigned header=u32(dib),bpp=u16(dib+14);
    if(header<40||header>124||header>length||u32(dib+4)!=w||u32(dib+8)!=h*2||u16(dib+12)!=1||
       (bpp!=24&&bpp!=32)||u32(dib+16)!=0)continue;
    unsigned stride=((w*bpp+31)/32)*4,maskStride=((w+31)/32)*4;
    const size_t needed=size_t(stride)*h+size_t(maskStride)*h;
    if(needed>length-header)continue;
    unsigned candidate=unsigned(w>48?w-48:48-w)+unsigned(h>48?h-48:48-h);
    if(candidate>=score)continue;
    score=candidate;best={dib+header,dib+header+stride*h,w,h,bpp,stride,maskStride};
  }
  return best.pixels!=nullptr;
}
inline bool decode(const Frame& f,uint8_t* bgra,size_t capacity){
  if(!f.pixels||!f.mask||!bgra||!f.w||!f.h||f.w>64||f.h>64||(f.bpp!=24&&f.bpp!=32)||capacity<size_t(f.w)*f.h*4)return false;
  bool alpha=false;
  if(f.bpp==32)for(unsigned y=0;y<f.h&&!alpha;++y)for(unsigned x=0;x<f.w;++x)if(f.pixels[y*f.stride+x*4+3]){alpha=true;break;}
  for(unsigned y=0;y<f.h;++y){
    const uint8_t* row=f.pixels+(f.h-1-y)*f.stride;const uint8_t* mask=f.mask+(f.h-1-y)*f.maskStride;
    for(unsigned x=0;x<f.w;++x){const uint8_t* source=row+x*(f.bpp/8);uint8_t* dest=bgra+(y*f.w+x)*4;
      memcpy(dest,source,3);dest[3]=(mask[x/8]&(0x80>>(x%8)))?0:alpha?source[3]:255;
    }
  }
  return true;
}
}
