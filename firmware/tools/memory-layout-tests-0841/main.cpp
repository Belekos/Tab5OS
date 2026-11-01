#include "Tab5MemoryLayout.h"
#include <sdkconfig.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cassert>
#include <cstdio>

struct Region {uint32_t caps;size_t total,free,minimum,largest;};
constexpr size_t K=1024;
// Model overlapping capability queries, including an external DMA pool. The
// HP pool has two disjoint regions, so its largest block cannot be subtracted
// from another pool's largest block or inferred from the sum of free bytes.
const Region regions[]={
  {MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT,200*K,40*K,30*K,17*K},
  {MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT,100*K,30*K,25*K,19*K},
  {MALLOC_CAP_INTERNAL|MALLOC_CAP_TCM|MALLOC_CAP_8BIT,8*K,6*K,4*K,5*K},
  {MALLOC_CAP_INTERNAL|MALLOC_CAP_RTCRAM|MALLOC_CAP_8BIT,24*K,8*K,3*K,7*K},
  {MALLOC_CAP_SPIRAM|MALLOC_CAP_DMA|MALLOC_CAP_8BIT,32*1024*K,20*1024*K,16*1024*K,12*1024*K}
};
unsigned walks=0;
size_t sum(uint32_t caps,size_t Region::*field){size_t n=0;for(const auto& r:regions)if((r.caps&caps)==caps)n+=r.*field;return n;}
size_t heap_caps_get_total_size(uint32_t c){return sum(c,&Region::total);}
size_t heap_caps_get_free_size(uint32_t c){return sum(c,&Region::free);}
size_t heap_caps_get_minimum_free_size(uint32_t c){return sum(c,&Region::minimum);}
size_t heap_caps_get_largest_free_block(uint32_t c){++walks;size_t n=0;for(const auto& r:regions)if((r.caps&c)==c)n=std::max(n,r.largest);return n;}
int main(){
  using namespace Tab5MemoryLayout;
  const auto light=sample();
  assert(!light.largestSampled&&walks==0);
  assert(light.internal.total==332*K&&light.internal.free==84*K);
  assert(light.psram.total==32*1024*K&&light.psram.free==20*1024*K);
  const auto full=sample(true);
  assert(full.largestSampled&&full.internal.largestFree==19*K);
  assert(full.psram.largestFree==12*1024*K);
#if CONFIG_IDF_TARGET_ESP32P4
  assert(full.splitAvailable&&walks==5);
  assert(full.hp.total==300*K&&full.hp.free==70*K&&full.hp.minimumFree==55*K);
  assert(full.hp.largestFree==19*K&&full.hp.used()==230*K);
  assert(full.tcm.total==8*K&&full.lp.total==24*K);
  assert(full.internal.total==full.hp.total+full.tcm.total+full.lp.total);
  assert(full.internal.free==full.hp.free+full.tcm.free+full.lp.free);
  assert(full.hpPhysicalBytes==768*K&&full.tcmPhysicalBytes==8*K&&full.lpPhysicalBytes==32*K);
  assert(full.l2CacheBytes==256*K);
#else
  assert(!full.splitAvailable&&walks==2);
  assert(full.hp.total==0&&full.tcm.total==0&&full.lp.total==0);
  assert(full.hpPhysicalBytes==0&&full.l2CacheBytes==0);
#endif
  Pool inconsistent;inconsistent.total=5;inconsistent.free=6;assert(inconsistent.used()==0);
  std::puts("capability pools: pass; periodic heap walks: 0");
}
