#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include "Tab5ResourceManager.h"
#include "Tab5HeapProbe.h"
#include "Tab5HeapFit.h"
#include "multi_heap.h"

using namespace Tab5Resources;
struct NativeLayout {void* lock;size_t free,min,pool;void* tlsf;};
static size_t checks=0,probes=0,fullSamples=0;
static multi_heap_handle_t pool;
static void* tlsfState(){return reinterpret_cast<NativeLayout*>(pool)->tlsf;}
static void check(bool value){++checks;assert(value);}
static bool probe(size_t n,size_t align=4){++probes;return tab5TlsfCanAllocate(tlsfState(),n,align);}
static MemoryTotals totals(){return {MiB,multi_heap_free_size(pool)};}
static MemorySample fullSample(){++fullSamples;assert(!"full heap sample on admission path");return {};}
static MemorySample sample(const Budget& requested){
  MemorySample result{MiB,MiB,MiB,multi_heap_free_size(pool),0};
  result.psramFit=!requested.psramBlock?MemorySample::Block::Unknown:
      probe(requested.psramBlock)?MemorySample::Block::Fit:MemorySample::Block::NoFit;
  return result;
}
static void differential(){
  constexpr size_t bytes=128*KiB;
  void* arena=std::aligned_alloc(128,bytes);check(arena!=nullptr);
  pool=multi_heap_register(arena,bytes);check(pool!=nullptr);
  std::vector<void*> allocations(200,nullptr);
  std::vector<unsigned char> before(bytes);
  std::mt19937 rng(849);
  for(unsigned step=0;step<5000;++step){
    const unsigned slot=rng()%allocations.size();
    if(allocations[slot])multi_heap_free(pool,allocations[slot]);
    allocations[slot]=multi_heap_malloc(pool,16+(rng()%600));
    const size_t alignment=size_t(4)<<(rng()%8);
    // Exercise both TLSF class boundaries and arbitrary fragmented requests.
    const size_t requested=step%2 ? (rng()%(192*KiB)) : (size_t(1)<<(rng()%17))+(rng()%17);
    const size_t overhead=step%3 ? 12 : 0;
    std::memcpy(before.data(),arena,bytes);
    const bool fits=probe(requested+overhead,alignment);
    check(std::memcmp(before.data(),arena,bytes)==0);
    void* actual=alignment<=4 ? tlsf_malloc(tlsfState(),requested+overhead) :
        tlsf_memalign_offs(tlsfState(),alignment,requested+overhead,overhead?8:0);
    check((actual!=nullptr)==fits);
    if(actual)tlsf_free(tlsfState(),actual);
  }
  for(auto p:allocations)if(p)multi_heap_free(pool,p);
  check(!probe(0));check(!probe(SIZE_MAX));check(!probe(1024,3));
  std::free(arena);
}
static size_t releaseCache(void* context){
  auto& blocks=*static_cast<std::vector<void*>*>(context);
  const size_t before=multi_heap_free_size(pool);
  for(auto& ptr:blocks)if(ptr){multi_heap_free(pool,ptr);ptr=nullptr;}
  return multi_heap_free_size(pool)-before;
}
static void managerIntegration(){
  constexpr size_t bytes=4*MiB;
  void* arena=std::aligned_alloc(128,bytes);check(arena!=nullptr);
  pool=multi_heap_register(arena,bytes);check(pool!=nullptr);
  std::vector<void*> blocks;
  while(void* p=multi_heap_malloc(pool,256*KiB))blocks.push_back(p);
  check(blocks.size()>10);
  for(size_t i=0;i<blocks.size();i+=2){multi_heap_free(pool,blocks[i]);blocks[i]=nullptr;}
  const Budget wanted{0,0,0,768*KiB,768*KiB};
  check(totals().psram>wanted.psram+PsramSafety);
  check(!probe(wanted.psramBlock));
  Manager manager(fullSample,totals,sample);
  manager.begin();
  check(manager.addCache(Owner::Map,"native fragmented heap",releaseCache,&blocks,{MemoryDomain::Psram}));
  AdmissionReport denied;
  auto first=manager.reserve(Owner::Audio,wanted,&denied);
  check(!first&&denied.reason==Denial::PsramBlock);
  check(denied.memory.psramFit==MemorySample::Block::NoFit);
  char message[512];describeAdmission(denied,message,sizeof(message));
  check(std::strstr(message,"bloc < 768 Kio")!=nullptr);
  AdmissionReport recovered;
  auto second=manager.reserveRecovering(Owner::Audio,wanted,true,&recovered);
  check(bool(second));check(recovered.reason==Denial::None);
  check(manager.collections()>0);
  void* actual=multi_heap_malloc(pool,wanted.psramBlock);check(actual!=nullptr);
  second.allocated(0,wanted.psramBlock);multi_heap_free(pool,actual);second.release();
  check(fullSamples==0);
  MemorySample unknown{MiB,0,0,MiB,0};
  unknown.internalFit=unknown.dmaFit=unknown.psramFit=MemorySample::Block::Unknown;
  check(Manager::fits(unknown,{0,32*KiB,8*KiB,0,64*KiB}));
  AdmissionReport report;report.memory=unknown;describeAdmission(report,message,sizeof(message));
  check(std::strstr(message,"bloc ? 0 Kio")!=nullptr);
  check(tab5HeapCanAllocate(4096,0)==TAB5_HEAP_FIT_UNKNOWN);
  std::free(arena);
}
int main(){differential();managerIntegration();
  std::printf("{\"passed\":true,\"checks\":%zu,\"native_fit_comparisons\":5000,\"probes\":%zu,\"full_heap_samples\":%zu}\n",checks,probes,fullSamples);
}
