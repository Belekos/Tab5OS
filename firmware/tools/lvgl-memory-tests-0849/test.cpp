#include <lvgl.h>
#include "Tab5LvglMemory.h"
#include <stdlib/builtin/lv_tlsf.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
#include <random>
#include <algorithm>

std::mutex heapMutex;
std::unordered_map<void*,size_t> heapBlocks;
size_t heapUsed=0,heapPeak=0,heapCalls=0,heapFrees=0;
size_t heapLimit=32U*1024*1024,largestLimit=SIZE_MAX;
std::atomic<bool> reject{false};
void* heap_caps_malloc(size_t bytes,unsigned){
 std::lock_guard<std::mutex> g(heapMutex);++heapCalls;
 if(reject || bytes>heapLimit-heapUsed || bytes>largestLimit)return nullptr;
 void* p=std::malloc(bytes);if(p){heapBlocks[p]=bytes;heapUsed+=bytes;heapPeak=std::max(heapPeak,heapUsed);}return p;
}
void heap_caps_free(void* p){if(!p)return;std::lock_guard<std::mutex>g(heapMutex);assert(heapBlocks.count(p));heapUsed-=heapBlocks[p];heapBlocks.erase(p);++heapFrees;std::free(p);}
extern "C" {
void* lv_malloc_core(size_t);void* lv_realloc_core(void*,size_t);void lv_free_core(void*);
lv_result_t lv_mem_test_core();void lv_mem_init();void lv_mem_deinit();
}
void payload(void* p,size_t n,unsigned c){assert(p);std::memset(p,c,n);}
void verify(void* p,size_t n,unsigned c){assert(p);for(size_t i=0;i<n;++i)assert(static_cast<unsigned char*>(p)[i]==c);}
void smallChurn(){
 std::vector<void*> blocks;
 const size_t before=heapCalls;
 for(unsigned i=0;i<10000;++i){size_t n=24+(i%23)*8;auto* p=lv_malloc_core(n);payload(p,n,i%251);blocks.push_back(p);}
 const auto occupied=tab5LvglMemoryStats();
 for(unsigned i=0;i<10000;++i)verify(blocks[i],24+(i%23)*8,i%251);
 std::mt19937 order(17);std::shuffle(blocks.begin(),blocks.end(),order);
 for(void* p:blocks)lv_free_core(p);
 assert(lv_mem_test_core()==LV_RESULT_OK);
 assert(tab5LvglMemoryStats().pools==0);
 printf("churn blocks=10000 backingCalls=%zu backingPeak=%zu liveUsed=%zu pools=%zu TLSF=%zu\n",heapCalls-before,occupied.ownedBytes,occupied.usedBytes,occupied.pools,lv_tlsf_size());
}
void reallocation(){
 auto* p=lv_malloc_core(4096);payload(p,4096,0x56);
 auto* smaller=lv_realloc_core(p,2048);assert(smaller==p);verify(smaller,2048,0x56);
 auto* extended=lv_realloc_core(smaller,8192);assert(extended==p);verify(extended,2048,0x56);payload(extended,8192,0x76);
 auto* blocker=lv_malloc_core(192*1024);payload(blocker,192*1024,0x17);
 auto* moved=lv_realloc_core(extended,400*1024);assert(moved&&moved!=extended);verify(moved,8192,0x76);
 reject=true;auto* failed=lv_realloc_core(moved,24*1024*1024);assert(!failed);verify(moved,8192,0x76);reject=false;
 assert(!lv_realloc_core(moved,SIZE_MAX));verify(moved,8192,0x76);
 verify(blocker,192*1024,0x17);lv_free_core(blocker);lv_free_core(moved);
 assert(tab5LvglMemoryStats().liveBlocks==0 && tab5LvglMemoryStats().pools==0);
 reject=true;assert(!lv_malloc_core(32));reject=false;
 assert(tab5LvglMemoryStats().pools==0);
 largestLimit=4096;auto* fallback=lv_malloc_core(2048);payload(fallback,2048,7);assert(fallback);lv_free_core(fallback);largestLimit=SIZE_MAX;
 assert(lv_mem_test_core()==LV_RESULT_OK);
 auto* large=lv_malloc_core(8*1024*1024);payload(large,8*1024*1024,0xa7);
 verify(large,8*1024*1024,0xa7);lv_free_core(large);
 assert(tab5LvglMemoryStats().pools==0);
}
void sizeClasses(){
 unsigned checked=0;
 for(unsigned bit=4;bit<=24;++bit)for(int delta:{-1,0,1,31,127}){
   size_t n=(size_t(1)<<bit)+delta;
   auto* p=static_cast<unsigned char*>(lv_malloc_core(n));assert(p);
   p[0]=0x1a;p[n-1]=0xe7;assert(lv_tlsf_block_size(p)>=n);
   assert(p[0]==0x1a && p[n-1]==0xe7);lv_free_core(p);
   assert(lv_mem_test_core()==LV_RESULT_OK);++checked;
 }
 assert(tab5LvglMemoryStats().pools==0);
 printf("sizeClasses cases=%u range=15..16777343 all released\n",checked);
}
void externalPool(){
 alignas(16) static unsigned char storage[64*1024];
 assert(!lv_mem_add_pool(storage+1,sizeof(storage)-1));
 assert(!lv_mem_add_pool(storage,2));
 auto pool=lv_mem_add_pool(storage,sizeof(storage));assert(pool);
 auto* p=lv_malloc_core(64);payload(p,64,9);
 lv_mem_remove_pool(pool);assert(tab5LvglMemoryStats().pools==1);verify(p,64,9);
 lv_free_core(p);assert(tab5LvglMemoryStats().pools==1);
 lv_mem_remove_pool(pool);assert(tab5LvglMemoryStats().pools==0);
}
void concurrent(){
 std::vector<std::thread> workers;
 for(unsigned t=0;t<4;++t)workers.emplace_back([t]{for(unsigned cycle=0;cycle<600;++cycle){
   size_t a=64+(cycle%1024),b=8*1024+(cycle%1536);void* p=lv_malloc_core(a);payload(p,a,t+1);
   void* q=lv_realloc_core(p,b);verify(q,a,t+1);payload(q,b,t+1);verify(q,b,t+1);lv_free_core(q);
 }});
 for(auto& w:workers)w.join();assert(lv_mem_test_core()==LV_RESULT_OK);
 assert(tab5LvglMemoryStats().pools==0);
}
void render(){
 lv_init();auto* d=lv_display_create(320,240);assert(d);
 alignas(16) static uint8_t buffer[320*16*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(d,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
 lv_display_set_flush_cb(d,[](lv_display_t* display,const lv_area_t*,uint8_t*){lv_display_flush_ready(display);});
 const auto initial=tab5LvglMemoryStats();
 size_t warmUsed=0;
 for(unsigned cycle=0;cycle<100;++cycle){auto* panel=lv_obj_create(lv_screen_active());
  for(unsigned row=0;row<60;++row){auto* label=lv_label_create(panel);lv_label_set_text_fmt(label,"Media %u — Élément %u",cycle,row);lv_obj_set_pos(label,row%4*60,row/4*18);}
  lv_tick_inc(20);lv_timer_handler();lv_obj_delete(panel);assert(lv_mem_test()==LV_RESULT_OK);
  const auto cycleStats=tab5LvglMemoryStats();
  if(cycle==2)warmUsed=cycleStats.usedBytes;
  if(cycle>2)assert(cycleStats.usedBytes==warmUsed);
 }
 const auto after=tab5LvglMemoryStats();
 printf("lvgl cycles=100 rows=6000 beforeUsed=%zu afterUsed=%zu beforePools=%zu afterPools=%zu\n",initial.usedBytes,after.usedBytes,initial.pools,after.pools);
 // Native LVGL caches may legitimately persist until its deinit.
 lv_deinit();assert(heapUsed==0);assert(heapBlocks.empty());
}
int main(){
 reject=true;lv_mem_init();assert(!lv_malloc_core(10));assert(heapUsed==0);reject=false;
 lv_mem_init();smallChurn();reallocation();sizeClasses();externalPool();concurrent();lv_mem_deinit();assert(heapUsed==0);
 render();
 printf("PASS native TLSF ownership, growth, OOM preservation, in-place/moving realloc, pool release, 4 threads, LVGL UI cycles; heapPeak=%zu\n",heapPeak);
}
