#include "Tab5MeshMemory.h"
#include "Tab5ResourceManager.h"
#include <esp_heap_caps.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>

extern "C" {
void* bt_osi_mem_malloc(size_t);
void* bt_osi_mem_calloc(size_t,size_t);
void* bt_osi_mem_malloc_internal(size_t);
void* bt_osi_mem_calloc_internal(size_t,size_t);
void bt_osi_mem_free(void*);
void bt_osi_mem_free_internal(void*);
}
static unsigned checks=0,allocationCalls=0,failCall=0;
static void check(bool value,const char* why){++checks;if(!value){fprintf(stderr,"FAIL %u: %s\n",checks,why);exit(1);}}
struct Allocation{size_t bytes;unsigned caps;};
static std::map<void*,Allocation> live;
static size_t internalBytes=0,externalBytes=0;
extern "C" void* heap_caps_malloc(size_t size,unsigned caps){
 ++allocationCalls;if(failCall==allocationCalls)return nullptr;
 void* p=malloc(size);if(p){live[p]={size,caps};if(caps&MALLOC_CAP_INTERNAL)internalBytes+=size;else externalBytes+=size;}return p;
}
extern "C" void* heap_caps_calloc(size_t count,size_t size,unsigned caps){
 if(count&&size>std::numeric_limits<size_t>::max()/count)return nullptr;
 void* p=heap_caps_malloc(count*size,caps);if(p)memset(p,0,count*size);return p;
}
extern "C" void heap_caps_free(void* p){
 if(!p)return;auto it=live.find(p);check(it!=live.end(),"valid SDK free");
 if(it->second.caps&MALLOC_CAP_INTERNAL)internalBytes-=it->second.bytes;else externalBytes-=it->second.bytes;
 live.erase(it);free(p);
}
static Tab5Resources::MemorySample sample;
static Tab5Resources::MemorySample memory(){return sample;}
static Tab5Resources::MemoryTotals totals(){return {sample.internal,sample.psram};}
int main(){
 using namespace Tab5Resources;
 check(TAB5_MESH_EXTERNAL_POOLS==1,"reviewed SDK selects external host pools");
 check(InternalSafety==32*KiB&&PsramSafety==512*KiB,"global safety margins preserved");
 const Budget selected{Tab5MeshMemory::Internal,Tab5MeshMemory::LargestBlock,0,Tab5MeshMemory::External,Tab5MeshMemory::ExternalBlock};
 const Budget previous{48*KiB,10*KiB,0,0,0};
 check(selected.internal==28*KiB&&selected.psram==44*KiB,"separate conservative heaps");
 // User's reported 275/347 KiB leaves 72 KiB. The worker's 8 KiB stack has
 // already been allocated when its host admission runs; it is not pending too.
 sample={72*KiB,32*KiB,32*KiB,8*MiB,4*MiB};
 Manager manager(memory,totals);AdmissionReport report;
 auto worker=manager.reserve(Owner::Tools,TaskBudget(8*KiB),&report);
 check(bool(worker),"worker creation admitted at reported boot profile");
 sample.internal-=8*KiB;worker.allocated(8*KiB,0);worker.release();
 auto old=manager.reserve(Owner::Tools,previous,&report);
 check(!old&&report.reason==Denial::Internal,"reproduce .40 rejection after worker stack allocation");
 auto current=manager.reserve(Owner::Tools,selected,&report);
 check(bool(current)&&report.pending.internal==0,".41 admits host without double counting worker");
 sample.internal-=selected.internal;sample.psram-=selected.psram;
 current.allocated(selected.internal,selected.psram);
 check(sample.internal>=InternalSafety&&sample.psram>=PsramSafety,"modeled complete peak retains safety margins");
 current.release();
 sample={60*KiB-1,8*KiB,8*KiB,8*MiB,4*MiB};
 check(Manager::rejection(sample,selected)==Denial::Internal,"real internal shortage remains refused");
 sample.internal=64*KiB;sample.internalBlock=6*KiB-1;
 check(Manager::rejection(sample,selected)==Denial::InternalBlock,"fragmented stack allocation refused");
 sample.internalBlock=6*KiB;sample.psram=512*KiB+44*KiB-1;
 check(Manager::rejection(sample,selected)==Denial::Psram,"external safety margin remains enforced");
 sample.psram=8*MiB;sample.psramBlock=10*KiB-1;
 check(Manager::rejection(sample,selected)==Denial::PsramBlock,"fragmented host pool allocation refused");
 sample.psramBlock=4*MiB;
 check(Manager::rejection(sample,selected,{8*KiB,0,0,0,0})==Denial::Internal,"concurrent startup quotas remain counted");
 // These calls link the actual copied ESP-IDF allocator C module, not a mock
 // of its policy. Only the heap boundary is simulated and observed below.
 void* host=bt_osi_mem_malloc(384);void* pool=bt_osi_mem_calloc(24,320);
 void* dma=bt_osi_mem_malloc_internal(512);void* dmaZero=bt_osi_mem_calloc_internal(4,128);
 check(host&&pool&&dma&&dmaZero,"all allocator paths available");
 check(live[host].caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)&&live[pool].caps==live[host].caps,"host malloc and calloc request only PSRAM 8BIT");
 check(live[dma].caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT|MALLOC_CAP_DMA)&&live[dmaZero].caps==live[dma].caps,"explicit internal allocations retain DMA and internal caps");
 check(internalBytes==1024&&externalBytes==8064,"host buffers do not charge internal heap");
 for(size_t i=0;i<24*320;++i)check(static_cast<unsigned char*>(pool)[i]==0,"host calloc remains zeroed");
 bt_osi_mem_free(host);bt_osi_mem_free(pool);bt_osi_mem_free_internal(dma);bt_osi_mem_free_internal(dmaZero);
 bt_osi_mem_free(nullptr);bt_osi_mem_free_internal(nullptr);
 check(live.empty()&&!internalBytes&&!externalBytes,"all SDK allocations freed including null frees");
 failCall=allocationCalls+1;check(!bt_osi_mem_calloc(24,320),"host calloc failure remains visible to SDK caller");
 failCall=allocationCalls+1;check(!bt_osi_mem_calloc_internal(4,128),"internal calloc failure remains visible");
 failCall=allocationCalls+1;check(!bt_osi_mem_malloc_internal(512),"internal malloc failure remains visible");
 failCall=0;check(!bt_osi_mem_calloc(std::numeric_limits<size_t>::max(),2),"calloc multiplication overflow refused by heap boundary");
 for(unsigned session=0;session<200;++session){
   void* first=bt_osi_mem_calloc(12,256);failCall=allocationCalls+1;
   void* second=bt_osi_mem_calloc(24,320);check(first&&!second,"pool failure injected after earlier allocation");
   bt_osi_mem_free(first);bt_osi_mem_free(second);failCall=0;
   check(live.empty()&&!internalBytes&&!externalBytes,"rollback frees previously allocated pool");
 }
 printf("{\"checks\":%u,\"passed\":true,\"originalAdmissionKiB\":48,\"selectedInternalKiB\":28,\"selectedExternalKiB\":44,\"oomRollbackCycles\":200,\"physicalDeviceTested\":false}\n",checks);
}
