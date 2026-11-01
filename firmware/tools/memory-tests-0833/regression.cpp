#include "Tab5ResourceManager.h"
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
using namespace Tab5Resources;
static size_t internalFree=256*KiB,externalFree=4*MiB;
static MemorySample sample(){return {internalFree,internalFree,internalFree,externalFree,externalFree};}
static MemoryTotals totals(){return {internalFree,externalFree};}
static void heap(size_t in=256*KiB,size_t ex=4*MiB){internalFree=in;externalFree=ex;}
struct Cache { unsigned calls=0; size_t in=0,ex=0; };
static size_t reclaim(void* p){auto& c=*static_cast<Cache*>(p);++c.calls;internalFree+=c.in;externalFree+=c.ex;return c.in+c.ex;}
static CachePolicy policy(MemoryDomain domain){
#ifdef BASELINE_0832
 return {domain,30000};
#else
 return {domain};
#endif
}
static void foreground(Manager& manager,Owner owner,uint32_t now){
#ifdef BASELINE_0832
 manager.foreground(owner,now);
#else
 (void)now;manager.foreground(owner);
#endif
}
static void service(Manager& manager,uint32_t now){
 manager.serviceCollections();
#ifdef BASELINE_0832
 manager.serviceMaintenance(now);
#else
 (void)now;
#endif
}
// These small hardware boundaries allow execution of the exact Arduino
// allocation-failure and resources-service bodies extracted by the runner.
static uint32_t simulatedMs=1;
static unsigned logs=0;
struct Serial { template<class... Args> void printf(const char*,Args...){++logs;} };
static Serial Tab5DebugSerial;
static unsigned xTaskGetCurrentTaskHandle(){return 1;}
static uint32_t millis(){return simulatedMs;}
static unsigned uiTask=1;
static std::atomic<size_t> failedBytes{0};
static std::atomic<uint32_t> failedCaps{0},failedCount{0};
constexpr uint32_t MALLOC_CAP_INTERNAL=1u<<0,MALLOC_CAP_SPIRAM=1u<<1,
 MALLOC_CAP_DMA=1u<<2,MALLOC_CAP_8BIT=1u<<3,MALLOC_CAP_DEFAULT=1u<<4;
Tab5Resources::Manager Tab5MemoryManager(sample,totals);
#include "resource_service.inc"
int main(){
 heap();Manager idle(sample,totals);Cache idleCache;idleCache.ex=KiB;
 idle.addCache(Owner::Map,"reloadable background tiles",reclaim,&idleCache,policy(MemoryDomain::Psram));
 foreground(idle,Owner::Map,1000);foreground(idle,Owner::Files,2000);
 service(idle,32000);
 const unsigned idleWithoutDemand=idleCache.calls;
 heap(46*KiB);Manager maintenance(sample,totals);Cache worker;worker.in=16*KiB;
 maintenance.addCache(Owner::Audio,"inactive worker",reclaim,&worker,policy(MemoryDomain::Internal));
 service(maintenance,1);
 const unsigned lowButIdle=worker.calls;
 heap(256*KiB,550*KiB);Manager foregroundManager(sample,totals);Cache visible;visible.ex=MiB;
 foregroundManager.addCache(Owner::Map,"visible tiles with no callback veto",reclaim,&visible,policy(MemoryDomain::Psram));
 foreground(foregroundManager,Owner::Map,1);foregroundManager.collect({0,0,0,128*KiB,0},Owner::Tools);
 const unsigned foregroundEvicted=visible.calls;
 heap(256*KiB,550*KiB);Manager requesterManager(sample,totals);Cache requested;requested.ex=MiB;
 requesterManager.addCache(Owner::Tools,"requesting operation state",reclaim,&requested,policy(MemoryDomain::Psram));
 foreground(requesterManager,Owner::Map,1);requesterManager.collect({0,0,0,128*KiB,0},Owner::Tools);
 const unsigned requesterEvicted=requested.calls;
 heap(46*KiB);Cache unrelatedWorker;unrelatedWorker.in=16*KiB;
 Tab5MemoryManager.addCache(Owner::Audio,"unrelated internal worker",reclaim,&unrelatedWorker,policy(MemoryDomain::Internal));
 foreground(Tab5MemoryManager,Owner::Audio,1);foreground(Tab5MemoryManager,Owner::Files,1);
 // Optional PSRAM allocation failed, caller has already recovered; 4MiB
 // is free. SRAM46KiB exceeds safety32KiB, and no new job is starting.
 allocationFailed(128*KiB,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT,"test");
 tab5ResourcesService();
 const unsigned staleFailureEvicted=unrelatedWorker.calls;
 const unsigned failureDiagnostic=logs;
 printf("{\"idleWithoutDemand\":%u,\"lowButIdle\":%u,\"foregroundEvicted\":%u,\"requesterEvicted\":%u,\"staleFailureEvicted\":%u,\"failureDiagnostic\":%u}\n",
   idleWithoutDemand,lowButIdle,foregroundEvicted,requesterEvicted,staleFailureEvicted,failureDiagnostic);
}
