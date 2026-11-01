#include "Tab5Memory.h"
#include "Tab5ResourceManager.h"
#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
Debug Tab5DebugSerial;
static unsigned checks;
static std::vector<uint32_t> scans;
static void check(bool ok,const char* why){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",why);abort();}}
void* heap_caps_malloc(size_t n,uint32_t){return malloc(n);}
void* heap_caps_calloc(size_t n,size_t s,uint32_t){return calloc(n,s);}
void* heap_caps_realloc(void* p,size_t n,uint32_t){return realloc(p,n);}
void heap_caps_free(void* p){free(p);}
size_t heap_caps_get_free_size(uint32_t c){return c&MALLOC_CAP_SPIRAM?8U*1024*1024:c&MALLOC_CAP_TCM?2U*1024:c&MALLOC_CAP_RTCRAM?4U*1024:c&MALLOC_CAP_DMA?64U*1024:70U*1024;}
size_t heap_caps_get_total_size(uint32_t c){return c&MALLOC_CAP_SPIRAM?32U*1024*1024:c&MALLOC_CAP_TCM?8U*1024:c&MALLOC_CAP_RTCRAM?32U*1024:c&MALLOC_CAP_DMA?300U*1024:340U*1024;}
size_t heap_caps_get_minimum_free_size(uint32_t c){return c&MALLOC_CAP_SPIRAM?4U*1024*1024:c&MALLOC_CAP_TCM?1024:c&MALLOC_CAP_RTCRAM?2U*1024:c&MALLOC_CAP_DMA?40U*1024:43U*1024;}
size_t heap_caps_get_largest_free_block(uint32_t c){scans.push_back(c);return c&MALLOC_CAP_SPIRAM?4U*1024*1024:c&MALLOC_CAP_TCM?1024:c&MALLOC_CAP_RTCRAM?3U*1024:48U*1024;}
int main(){using namespace Tab5Resources;
  for(unsigned mask=0;mask<8;++mask){scans.clear();Budget b{0,mask&1?1U:0U,mask&2?1U:0U,0,mask&4?1U:0U};auto s=tab5MemorySample(b);
    check(s.internal==70U*1024&&s.psram==8U*1024*1024,"counter totals retain separate capabilities");
    std::vector<uint32_t> expected;if(mask&1)expected.push_back(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);if(mask&2)expected.push_back(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT);if(mask&4)expected.push_back(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    check(scans==expected,"only required block capabilities traversed");
    check(s.internalBlock==((mask&1)?48U*1024:0)&&s.dmaBlock==((mask&2)?48U*1024:0)&&s.psramBlock==((mask&4)?4U*1024*1024:0),"returned block sizes match requested domain");
  }
  scans.clear();auto full=tab5MemorySample();check(scans.size()==3&&full.internalBlock&&full.dmaBlock&&full.psramBlock,"explicit diagnostic remains complete");
  scans.clear();auto counts=tab5MemoryTotals();check(scans.empty()&&counts.internal==full.internal&&counts.psram==full.psram,"cheap counter path never traverses blocks");
  scans.clear();check(!tab5DumpMemory(nullptr,nullptr)&&scans.empty(),"null output performs no diagnostic heap traversal");
  std::string dump;auto emit=[](const char* p,size_t n,void* c){check(n<240,"diagnostic lines bounded");static_cast<std::string*>(c)->append(p,n);};
  check(tab5DumpMemory(emit,&dump),"explicit detailed memory diagnostic available");
  check(scans.size()==5,"explicit P4 diagnostic samples each heap domain once");
  check(dump.find("HP SRAM heap 307200 65536 40960 49152")!=std::string::npos,"HP row has total/free/minimum/largest");
  check(dump.find("SPM/TCM heap 8192 2048 1024 1024")!=std::string::npos,"SPM/TCM represented once");
  check(dump.find("LP SRAM heap 32768 4096 2048 3072")!=std::string::npos,"LP row distinct");
  check(dump.find("physical HP=786432 SPM/TCM=8192 LP=32768 L2_cache=262144 HP_code_data_reserved=217088")!=std::string::npos,"physical and usable capacities separated");
  check(dump.find("PSRAM heap 33554432 8388608 4194304 4194304")!=std::string::npos,"PSRAM row distinct");
  printf("{\"passed\":true,\"checks\":%u}\n",checks);
}
