#include "Tab5DisplayBlit.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

static uint32_t seed=0xd4634751;
static uint32_t random32(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}

// Independent scalar transforms: one explicit orthogonal transform per mode.
static void physical(unsigned r,int x,int y,int pw,int ph,int& px,int& py){
 switch(r){
 case 0:px=x;py=y;break;
 case 1:px=pw-1-y;py=x;break;
 case 2:px=pw-1-x;py=ph-1-y;break;
 case 3:px=y;py=ph-1-x;break;
 case 4:px=x;py=ph-1-y;break;
 case 5:px=y;py=x;break;
 case 6:px=pw-1-x;py=y;break;
 default:px=pw-1-y;py=ph-1-x;break;
 }
}

int main(){
 using namespace Tab5DisplayCacheRanges;
 unsigned cases=0,rejections=0;uint64_t pixels=0;
 for(unsigned r=0;r<8;++r)for(unsigned i=0;i<1000;++i){
  const int pw=1+random32()%111,ph=1+random32()%123;
  const size_t stride=(pw*2+3)&~size_t(3),bytes=stride*ph;
  Geometry g{};assert(configure(g,0x40000000,pw,ph,stride,r,0));
  const int x=random32()%g.logicalWidth,y=random32()%g.logicalHeight;
  const int w=1+random32()%(g.logicalWidth-x),h=1+random32()%(g.logicalHeight-y);
  const size_t inputStride=size_t(w)*2+random32()%39;
  const size_t extent=size_t(h-1)*inputStride+w*2;
  // Exactly the last pixel is available: no full final-row padding is required.
  std::vector<uint8_t> input(extent+1,0xa7);auto* source=input.data()+1;
  std::vector<uint8_t> out(bytes,0x6c),expected=out;
  for(int cy=0;cy<h;++cy)for(int cx=0;cx<w;++cx){
   const uint16_t value=uint16_t(random32());const size_t s=cy*inputStride+cx*2;
   source[s]=value>>8;source[s+1]=value;
   int px,py;physical(r,x+cx,y+cy,pw,ph,px,py);
   expected[size_t(py)*stride+px*2]=uint8_t(value);
   expected[size_t(py)*stride+px*2+1]=uint8_t(value>>8);
  }
  assert(Tab5DisplayBlit::copy(out.data(),bytes,g,x,y,w,h,source,extent,inputStride));
  assert(out==expected);++cases;pixels+=uint64_t(w)*h;
  auto before=out;
  assert(!Tab5DisplayBlit::copy(out.data(),bytes,g,x,y,w,h,source,extent-1,inputStride));++rejections;
  assert(out==before);
  assert(!Tab5DisplayBlit::copy(out.data(),bytes,g,x,y,w,h,source,extent,size_t(w)*2-1));++rejections;
  assert(out==before);
  assert(!Tab5DisplayBlit::copy(out.data(),bytes-1,g,x,y,w,h,source,extent,inputStride));++rejections;
  assert(out==before);
  auto bad=g;bad.logicalHeight=uint16_t(g.logicalHeight+1);
  assert(!Tab5DisplayBlit::copy(out.data(),bytes,bad,x,y,w,h,source,extent,inputStride));++rejections;
  assert(out==before);
 }
 std::printf("{\"passed\":true,\"transforms\":8,\"exactExtentCases\":%u,\"rejectionsWithoutMutation\":%u,\"pixels\":%llu,\"unalignedInput\":true,\"hardwareTest\":false}\n",cases,rejections,(unsigned long long)pixels);
}
