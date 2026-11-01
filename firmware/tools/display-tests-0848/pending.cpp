#include "Tab5DisplayPendingMirror.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
static unsigned checks, copies, overflows; static uint32_t seed=0x482457aa;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);abort();}}while(0)
static unsigned random(unsigned bound){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed%bound;}
int main(){
 using namespace Tab5DisplayPendingMirror;
 for(unsigned cycle=0;cycle<20000;++cycle){
  std::array<uint16_t,720> front{},back{},expected{};std::array<bool,720> changed{};
  unsigned first=random(720),end=first+1+random(720-first);
  for(unsigned i=0;i<720;++i){front[i]=expected[i]=uint16_t(i+cycle);back[i]=(i>=first&&i<end)?uint16_t(front[i]^0xffff):front[i];}
  Row row{};reset(row,first,end);
  auto copy=[&](uint16_t a,uint16_t b){++copies;CHECK(a<b&&b<=720);for(unsigned i=a;i<b;++i){CHECK(!changed[i]);back[i]=front[i];}};
  unsigned n=1+random(30);
  for(unsigned w=0;w<n;++w){
   unsigned a=random(720),b=a+1+random(720-a);unsigned before=copies;
   overwrite(row,a,b,copy);if(copies>before)++overflows;
   for(unsigned i=a;i<b;++i){changed[i]=true;back[i]=expected[i]=uint16_t(0x8000+w);}
  }
  flush(row,copy);CHECK(back==expected);CHECK(row.spans[0].first==0&&row.spans[1].end==0);
 }
 CHECK(overflows>0);
 printf("{\"checks\":%u,\"rowTransactions\":20000,\"copies\":%u,\"fragmentationFlushes\":%u}\n",checks,copies,overflows);
}
