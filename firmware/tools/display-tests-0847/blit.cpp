#include "Tab5DisplayBlit.h"
#include <lgfx/v1/misc/pixelcopy.hpp>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <chrono>

static uint64_t checks=0, rectangles=0, pixels=0;
#define CHECK(x) do { ++checks; if (!(x)) {std::cerr << "check " << __LINE__ << ": " << #x << "\n"; abort();} } while(0)

namespace lgfx { inline namespace v1 {
// Only the panel/hardware shell is adapted. The three methods below are
// extracted byte-for-byte from pinned M5GFX 0.2.30 for the reference renderer.
struct Panel_FrameBufferBase {
 struct Config {uint16_t panel_width=720,panel_height=1280;uint8_t offset_rotation=0;} _cfg;
 struct Range {int32_t left=INT32_MAX,right=0,top=INT32_MAX,bottom=0;} _range_mod;
 uint8_t _rotation=0,_internal_rotation=0,_write_bits=16;
 uint_fast16_t _width=720,_height=1280,_xe=719,_ye=1279,_xs=0,_ys=0;
 uint8_t** _lines_buffer=nullptr;
 void setRotation(uint_fast8_t);
 void _rotate_pixelcopy(uint_fast16_t&,uint_fast16_t&,uint_fast16_t&,uint_fast16_t&,pixelcopy_t*,uint32_t&,uint32_t&);
 void writeImage(uint_fast16_t,uint_fast16_t,uint_fast16_t,uint_fast16_t,pixelcopy_t*,bool);
};
// M5_METHODS
}}

static void reference(lgfx::Panel_FrameBufferBase& panel,int x,int y,int w,int h,const uint8_t* source) {
 lgfx::pixelcopy_t copy;
 copy.src_data=source;copy.src_bitwidth=w;copy.src_width=w;copy.src_height=h;
 copy.src_depth=lgfx::rgb565_2Byte;copy.dst_depth=lgfx::rgb565_nonswapped;
 copy.fp_copy=lgfx::pixelcopy_t::copy_rgb_affine<lgfx::rgb565_t,lgfx::swap565_t>;
 copy.fp_skip=lgfx::pixelcopy_t::skip_rgb_affine<lgfx::swap565_t>;
 copy.no_convert=false;
 panel.writeImage(x,y,w,h,&copy,false);
}

int main() {
 uint32_t random=0x38474631;
 auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
 for(unsigned scale=0;scale<2;++scale) {
  const unsigned physicalWidth=scale?720:23,physicalHeight=scale?1280:39;
  const size_t stride=physicalWidth*2+ (scale?0:6), bytes=stride*physicalHeight;
  std::vector<uint8_t> expected(bytes,0x35),actual(bytes,0x35);
  std::vector<uint8_t*> rows(physicalHeight);
  for(unsigned y=0;y<physicalHeight;++y)rows[y]=expected.data()+y*stride;
  lgfx::Panel_FrameBufferBase panel;panel._cfg.panel_width=physicalWidth;panel._cfg.panel_height=physicalHeight;panel._lines_buffer=rows.data();
  for(unsigned rotation=0;rotation<8;++rotation)for(unsigned offset=0;offset<8;++offset) {
   panel._cfg.offset_rotation=offset;panel.setRotation(rotation);
   Tab5DisplayCacheRanges::Geometry geometry;
   CHECK(Tab5DisplayCacheRanges::configure(geometry,0x40000000,physicalWidth,physicalHeight,stride,rotation,offset));
   for(unsigned n=0;n<(scale?64:128);++n) {
    const int x=next()%geometry.logicalWidth,y=next()%geometry.logicalHeight;
    const int w=1+next()%(geometry.logicalWidth-x),h=1+next()%(geometry.logicalHeight-y);
    std::vector<uint8_t> storage(size_t(w)*h*2+1);
    auto* source=storage.data()+1; // unaligned input is part of the port contract
    for(int k=0;k<w*h;++k){uint16_t color=uint16_t(next());source[k*2]=uint8_t(color>>8);source[k*2+1]=uint8_t(color);}
    // Upstream uses uint16_t access: give its reference an aligned copy.
    std::vector<uint16_t> aligned(size_t(w)*h);memcpy(aligned.data(),source,size_t(w)*h*2);
    reference(panel,x,y,w,h,reinterpret_cast<const uint8_t*>(aligned.data()));
    CHECK(Tab5DisplayBlit::copy(actual.data(),bytes,geometry,x,y,w,h,source,size_t(w)*h*2));
    CHECK(expected==actual);++rectangles;pixels+=uint64_t(w)*h;
    const size_t paddedStride=size_t(w)*2+3;
    std::vector<uint8_t> padded(paddedStride*size_t(h),0x8e);
    for(int row=0;row<h;++row)memcpy(padded.data()+size_t(row)*paddedStride,source+size_t(row)*w*2,size_t(w)*2);
    CHECK(Tab5DisplayBlit::copy(actual.data(),bytes,geometry,x,y,w,h,padded.data(),padded.size(),paddedStride));
    CHECK(expected==actual);
   }
   const auto before=actual;
   uint8_t input[8]={0xf8,0x00,0x07,0xe0,0x00,0x1f,0xff,0xff};
   CHECK(!Tab5DisplayBlit::copy(actual.data(),bytes,geometry,-1,0,1,1,input,8));
   CHECK(!Tab5DisplayBlit::copy(actual.data(),bytes,geometry,0,0,2,2,input,7));
   CHECK(!Tab5DisplayBlit::copy(actual.data(),bytes-1,geometry,0,0,1,1,input,8));
   CHECK(!Tab5DisplayBlit::copy(actual.data()+1,bytes-1,geometry,0,0,1,1,input,8));
   CHECK(!Tab5DisplayBlit::copy(actual.data(),bytes,geometry,geometry.logicalWidth,0,1,1,input,8));
   CHECK(!Tab5DisplayBlit::copy(actual.data(),bytes,geometry,0,geometry.logicalHeight,1,1,input,8));
   CHECK(!Tab5DisplayBlit::copy(actual.data(),bytes,geometry,0,0,INT32_MAX,INT32_MAX,input,8));
   CHECK(!Tab5DisplayBlit::copy(actual.data(),bytes,geometry,0,0,2,2,input,8,SIZE_MAX));
   CHECK(!Tab5DisplayBlit::copy(actual.data(),bytes,geometry,0,0,2,2,input,8,3));
   CHECK(actual==before);
  }
 }
 std::cout << "{\"checks\":"<<checks<<",\"rotationAndOffsetCases\":128,\"rectangles\":"<<rectangles<<",\"comparedPixels\":"<<pixels<<",\"reference\":\"M5GFX0.2.30 exact rotation, writeImage and RGB565 pixelcopy\"}" << std::endl;
}
