#include "Tab5Git.h"
#include "Tab5GitMemory.h"
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <stdexcept>
#include <cstdint>
using namespace Tab5Resources;
static MemorySample memory;
static size_t initialFree=0, allocationCalls=0, admissionCalls=0, checks=0;
static bool cancelNow=false, failHeap=false;
static std::map<void*,size_t> blocks;
MemorySample sample(){return memory;}
MemoryTotals totals(){return {memory.internal,memory.psram};}
Tab5Resources::Manager Tab5MemoryManager(sample,totals);
Tab5Resources::Reservation tab5ReserveMemory(Owner owner,const Budget& budget,const char*,AdmissionReport* report){
 ++admissionCalls;return Tab5MemoryManager.reserveRecovering(owner,budget,true,report);
}
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
void* heap_caps_malloc(size_t bytes,int caps){
 if(caps!=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))throw std::runtime_error("internal heap fallback");
 ++allocationCalls;if(failHeap||bytes>memory.psram||bytes>memory.psramBlock)return nullptr;
 void* p=std::malloc(bytes);if(p){blocks[p]=bytes;memory.psram-=bytes;}return p;
}
void heap_caps_free(void* p){auto it=blocks.find(p);if(it==blocks.end())throw std::runtime_error("unknown free");memory.psram+=it->second;blocks.erase(it);std::free(p);}
bool tab5_git_cancelled(){return cancelNow;}
namespace Tab5Git {
#include "allocator.inc"
Budget startupBudget(bool networkOperation){
#include "startup.inc"
 return budget;
}
}
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void reset(size_t psram=16*MiB,size_t block=16*MiB){
 check(blocks.empty()&&Tab5Git::current==0,"all physical and tracked blocks released");
 memory={160*KiB,80*KiB,80*KiB,psram,block};initialFree=psram;cancelNow=false;failHeap=false;
 Tab5Git::peak=0;Tab5Git::allocationDenied=false;Tab5Git::allocationFailure={};
}
struct SessionBudget {
 Tab5Git::AllocationBudget budget;
 SessionBudget(){check(Tab5Git::allocationBudget==nullptr,"one live Git budget");Tab5Git::allocationBudget=&budget;}
 ~SessionBudget(){Tab5Git::allocationBudget=nullptr;}
};
void noReservations(){
 auto whole=Tab5MemoryManager.reserve(Owner::Video,{0,0,0,memory.psram-PsramSafety,0});
 check(bool(whole),"no stale Git batch pending after session");
}
int main(){try{
 reset(4*MiB,2*MiB);
 auto startup=Tab5Git::startupBudget(true);auto previous=startup;previous.psram+=Tab5Git::MemoryLimit;
 check(!Manager::fits(memory,previous),"regression: old all-8-MiB admission rejects 4-MiB free");
 check(Manager::fits(memory,startup),"new startup admits modest TLS workspace");
 check(startup.internal==TlsBudget.internal+8*KiB&&startup.psram==TlsBudget.psram+128*KiB,"TLS and adapter startup headroom unchanged");
 {
  auto tls=Tab5MemoryManager.reserve(Owner::Tools,startup);check(bool(tls),"startup reserve");
  SessionBudget arena;std::vector<void*> data;
  const auto before=admissionCalls;
  for(unsigned n=0;n<600;++n){auto* p=Tab5Git::allocate(1024,__FILE__,__LINE__);check(p!=nullptr,"small repository working set can grow below old 8-MiB threshold");std::memset(p,int(n%256),1024);data.push_back(p);}
  check(admissionCalls-before<=4,"admission batches avoid per-object heap scans");
  check(Tab5Git::current.load()<MiB,"small-repository allocation stays below 1 MiB");
  auto other=Tab5MemoryManager.reserve(Owner::Map,{0,0,0,MiB,128*KiB});check(bool(other),"another app can use real remaining PSRAM");other.release();
  for(unsigned n=0;n<data.size();++n){check(static_cast<unsigned char*>(data[n])[0]==n%256,"buffer data intact");Tab5Git::release(data[n]);}
  check(memory.psram==initialFree,"all physical bytes returned before session close");
 }
 noReservations();
 // Pending budgets of concurrent apps remain protected as Git grows.
 reset(3*MiB,3*MiB);
 {
  auto other=Tab5MemoryManager.reserve(Owner::Map,{0,0,0,2*MiB,0});check(bool(other),"reserve another app startup");
  SessionBudget arena;auto* p=Tab5Git::allocate(400*KiB,__FILE__,__LINE__);check(p,"first Git allocation fits with concurrent reserve");
  check(!Tab5Git::allocate(160*KiB,__FILE__,__LINE__),"Git cannot consume another app reserve or system margin");
  check(Tab5Git::allocationFailure.reason==Denial::Psram,"concurrent-budget refusal explains PSRAM");
  Tab5Git::release(p);
 }
 noReservations();
 // A small final allocation must not require a full 256 KiB batch.
 reset(PsramSafety+128,1024);
 {
  SessionBudget arena;auto* p=Tab5Git::allocate(128-sizeof(Tab5Git::Allocation),__FILE__,__LINE__);
  check(p!=nullptr,"exact allocation fallback after batch admission fails");check(memory.psram==PsramSafety,"system safety margin remains intact");
  check(!Tab5Git::allocate(1,__FILE__,__LINE__),"refuse crossing system safety");Tab5Git::release(p);
 }
 noReservations();
 // Distinguish total free bytes, block fragmentation and internal SRAM.
 reset(4*MiB,128);
 {SessionBudget arena;check(!Tab5Git::allocate(512,__FILE__,__LINE__),"fragmented PSRAM fails safely");check(Tab5Git::allocationFailure.reason==Denial::PsramBlock,"PSRAM block diagnostic");}
 reset();memory.internal=31*KiB;
 {SessionBudget arena;check(!Tab5Git::allocate(32,__FILE__,__LINE__),"PSRAM allocation respects internal safety");check(Tab5Git::allocationFailure.reason==Denial::Internal,"internal shortage diagnostic");}
 reset();memory.internal=75*KiB;
 check(Manager::rejection(memory,TaskBudget(Tab5Git::WorkerStackBytes))==Denial::Internal,"40-KiB stack still needs 76 KiB internal including guard");
 memory.internal=100*KiB;memory.internalBlock=39*KiB;
 check(Manager::rejection(memory,TaskBudget(Tab5Git::WorkerStackBytes))==Denial::InternalBlock,"40-KiB stack cannot use fragmented internal memory");
 memory.internalBlock=80*KiB;memory.internal=87*KiB;
 check(Manager::rejection(memory,Tab5Git::startupBudget(true))==Denial::Internal,"internal TLS fallback requires full 88-KiB startup margin");
 Budget externalTls{16*KiB,4*KiB,0,384*KiB,128*KiB};memory.internal=47*KiB;
 check(Manager::rejection(memory,externalTls)==Denial::Internal,"external TLS still preserves 48-KiB internal startup margin");
 // Physical allocator failure, cancellation, overflow and exact 8-MiB quota.
 reset();
 {SessionBudget arena;failHeap=true;check(!Tab5Git::allocate(512,__FILE__,__LINE__),"physical OOM returns failure");check(Tab5Git::current==0,"failed allocation uncharged");failHeap=false;auto* p=Tab5Git::allocate(512,__FILE__,__LINE__);check(p,"retry after physical OOM");Tab5Git::release(p);}
 noReservations();reset();
 {SessionBudget arena;const auto before=admissionCalls;cancelNow=true;check(!Tab5Git::allocate(512,__FILE__,__LINE__),"cancellation allocates nothing");cancelNow=false;check(!Tab5Git::allocate(SIZE_MAX,__FILE__,__LINE__),"allocation size overflow rejected");check(admissionCalls==before,"cancel and overflow need no heap admission");}
 noReservations();reset();
 {SessionBudget arena;auto* p=Tab5Git::allocate(Tab5Git::MemoryLimit-sizeof(Tab5Git::Allocation),__FILE__,__LINE__);check(p,"exact total quota allocates");check(Tab5Git::current==Tab5Git::MemoryLimit,"headers included in quota");check(!Tab5Git::allocate(1,__FILE__,__LINE__),"quota cannot be exceeded");Tab5Git::release(p);}
 noReservations();reset();
 {SessionBudget arena;auto* p=Tab5Git::allocate(5*MiB,__FILE__,__LINE__);check(p,"resize original allocation");std::memset(p,0x5a,256);check(!Tab5Git::resize(p,4*MiB,__FILE__,__LINE__),"resize transient old+new respects quota");check(static_cast<unsigned char*>(p)[255]==0x5a,"failed resize preserves old data");auto* q=Tab5Git::resize(p,MiB,__FILE__,__LINE__);check(q&&static_cast<unsigned char*>(q)[255]==0x5a,"successful resize copies old prefix");check(Tab5Git::resize(q,0,__FILE__,__LINE__)==nullptr,"zero resize frees original");}
 noReservations();
 // Hundreds of successful and refused sessions must return all reservation slots.
 for(unsigned n=0;n<400;++n){reset();{SessionBudget arena;std::vector<void*> data;for(unsigned i=0;i<12;++i){auto* p=Tab5Git::allocate((i+1)*4096,__FILE__,__LINE__);check(p,"repeated allocation");data.push_back(p);}cancelNow=true;check(!Tab5Git::allocate(128,__FILE__,__LINE__),"repeat cancellation");cancelNow=false;for(auto* p:data)Tab5Git::release(p);}noReservations();}
 reset();printf("{\"passed\":true,\"checks\":%zu,\"repeatedSessions\":400,\"admissionCalls\":%zu,\"physicalAllocationCalls\":%zu}\n",checks,admissionCalls,allocationCalls);return 0;
 }catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
