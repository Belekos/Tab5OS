#include "Tab5TileValidation.h"
#include "Tab5TilePolicy.h"
#include "Tab5Cooperate.h"
#include "src/tab5png/tab5_pngle.h"
#include <lgfx/utility/lgfx_tjpgd.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#ifdef ARDUINO_ARCH_ESP32
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#ifdef TAB5_TILE_TEST_ALLOC
extern "C" void* tab5tile_test_alloc(size_t);
#endif
namespace Tab5TileValidation {
namespace {
void* allocate(size_t n) {
#ifdef ARDUINO_ARCH_ESP32
 return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif defined(TAB5_TILE_TEST_ALLOC)
 return tab5tile_test_alloc(n);
#else
 return std::malloc(n);
#endif
}
void release(void* p) {
#ifdef ARDUINO_ARCH_ESP32
 heap_caps_free(p);
#else
 std::free(p);
#endif
}
struct Context {
 const uint8_t* data; size_t size, position=0, pixels=0; uint16_t* output;
 bool (*cancelled)(); bool stopped=false, invalid=false; Tab5TimeSlice slice;
 bool cooperate() {
#ifdef ARDUINO_ARCH_ESP32
  if (slice.due(millis())) vTaskDelay(1);
#endif
  if (cancelled && cancelled()) stopped=true;
  return !stopped;
 }
};
uint32_t read(void* opaque,uint8_t* out,uint32_t count) {
 auto& c=*static_cast<Context*>(opaque); if (!c.cooperate()) return 0;
 const size_t n=std::min(size_t(count),c.size-c.position);
 if (out) std::memcpy(out,c.data+c.position,n); c.position+=n; return uint32_t(n);
}
uint16_t rgb565(unsigned r,unsigned g,unsigned b) {return uint16_t((r&248)<<8|(g&252)<<3|b>>3);}
void pngBlock(void* opaque,uint32_t x,uint32_t y,uint_fast8_t step,size_t n,const uint8_t* argb) {
 auto& c=*static_cast<Context*>(opaque); if (!c.cooperate()) return;
 if (!argb || !step || x>=256 || y>=256 || !n || n-1>(255-x)/step) {c.invalid=true;return;}
 c.pixels+=n;
 if (!c.output) return;
 for (size_t i=0;i<n;++i,x+=step,argb+=4) {
  unsigned alpha=argb[0],inv=255-alpha;
  c.output[size_t(y)*256+x]=rgb565((argb[1]*alpha+255*inv+127)/255,
    (argb[2]*alpha+255*inv+127)/255,(argb[3]*alpha+255*inv+127)/255);
 }
}
uint32_t jpegBlock(void* opaque,void* bitmap,JRECT* r) {
 auto& c=*static_cast<Context*>(opaque); if (!c.cooperate()) return 0;
 if (!bitmap || r->right<r->left || r->bottom<r->top || r->right>=256 || r->bottom>=256) {c.invalid=true;return 0;}
 c.pixels+=size_t(r->right-r->left+1)*(r->bottom-r->top+1);
 if (c.output) {
  auto* rgb=static_cast<const uint8_t*>(bitmap);
  for (unsigned y=r->top;y<=r->bottom;++y) for (unsigned x=r->left;x<=r->right;++x,rgb+=3)
   c.output[size_t(y)*256+x]=rgb565(rgb[0],rgb[1],rgb[2]);
 }
 return 1;
}
}
Result decode(const uint8_t* data,size_t size,uint16_t* pixels,bool (*cancelled)()) {
 if (cancelled && cancelled()) return Result::Unavailable;
 if (!data || size<8 || size>tab5tile::MaxBytes) return Result::Invalid;
 Context c{data,size,0,0,pixels,cancelled};
 if (!std::memcmp(data,"\x89PNG\r\n\x1a\n",8)) {
  if (!tab5tile::validPng(data,size)) return Result::Invalid;
  auto* decoder=tab5_pngle_new(); if (!decoder) return Result::NoMemory;
  int result=tab5_pngle_prepare(decoder,read,&c);
  if (!result) result=tab5_pngle_decomp(decoder,pngBlock);
  tab5_pngle_destroy(decoder);
  if (c.stopped) return Result::Unavailable;
  if (result==-3) return Result::NoMemory;
  return !result && !c.invalid && c.pixels==256U*256U ? Result::Valid : Result::Invalid;
 }
 if (data[0]==0xff && data[1]==0xd8) {
  // TJpgDec can finish the last MCU without asking for EOI. Require it too.
  if (size<4 || data[size-2]!=0xff || data[size-1]!=0xd9) return Result::Invalid;
  void* pool=allocate(4096); if (!pool) return Result::NoMemory;
  lgfxJdec decoder{}; auto result=lgfx_jd_prepare(&decoder,read,pool,4096,&c);
  if (result==JDR_OK && (decoder.width!=256 || decoder.height!=256)) {release(pool);return Result::Invalid;}
  if (result==JDR_OK) result=lgfx_jd_decomp(&decoder,jpegBlock,0);
  release(pool);
  if (c.stopped) return Result::Unavailable;
  // A valid unsupported JPEG variant is unavailable, not proven corrupt.
  if (result==JDR_FMT3) return Result::Unavailable;
  if (result==JDR_MEM1 || result==JDR_MEM2) return Result::NoMemory;
  return result==JDR_OK && !c.invalid && c.pixels==256U*256U ? Result::Valid : Result::Invalid;
 }
 return Result::Invalid;
}
Result validateSource(const Source& source,bool (*cancelled)()) {
 if (cancelled && cancelled()) return Result::Unavailable;
 if (!source.read) return Result::Unavailable;
 if (source.size>tab5tile::MaxBytes) return Result::Unavailable;
 if (!source.size) return Result::Invalid;
 auto* bytes=static_cast<uint8_t*>(allocate(source.size)); if(!bytes)return Result::NoMemory;
 size_t at=0; Result result=Result::Unavailable;
 while(at<source.size) {
  if(cancelled && cancelled())break;
  const size_t ask=std::min(size_t(32768),source.size-at);
  const size_t got=source.read(source.context,bytes+at,ask);
  if(!got || got>ask)break;
  at+=got;
#ifdef ARDUINO_ARCH_ESP32
  vTaskDelay(1);
#endif
 }
 if(at==source.size)result=validate(bytes,source.size,cancelled);
 release(bytes);return result;
}
Result validate(const uint8_t* data,size_t size,bool (*cancelled)()) {return decode(data,size,nullptr,cancelled);}
}
