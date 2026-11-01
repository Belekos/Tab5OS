#include "Tab5ResourceManager.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
using namespace Tab5Resources;
static unsigned checks=0;
static void check(bool value,const char* why){++checks;if(!value){fprintf(stderr,"FAIL %s\n",why);std::abort();}}
static std::atomic<size_t> inFree{0},inBlock{0},dmaBlock{0},exFree{0},exBlock{0};
static MemorySample sample(){return {inFree.load(),inBlock.load(),dmaBlock.load(),exFree.load(),exBlock.load()};}
static MemoryTotals totals(){return {inFree.load(),exFree.load()};}
static void heap(size_t in=256*KiB,size_t ex=4*MiB){inFree=in;inBlock=in;dmaBlock=in;exFree=ex;exBlock=ex;}
struct Cache {
 int id=0;size_t in=0,ex=0,inLargest=0,dmaLargest=0,exLargest=0;bool live=false;
 unsigned calls=0;std::thread::id calledOn{};std::vector<int>* order=nullptr;
 Manager* manager=nullptr;std::atomic<bool>* notify=nullptr;
};
static size_t reclaim(void* p){auto& c=*static_cast<Cache*>(p);++c.calls;c.calledOn=std::this_thread::get_id();
 if(c.order)c.order->push_back(c.id);if(c.live)return 0;
 if(c.notify)c.notify->store(true,std::memory_order_release);
 if(c.manager)c.manager->collect({16*KiB,0,0,0,0}); // nested collection must be inert
 inFree.fetch_add(c.in);exFree.fetch_add(c.ex);
 if(c.inLargest)inBlock=c.inLargest;if(c.dmaLargest)dmaBlock=c.dmaLargest;if(c.exLargest)exBlock=c.exLargest;
 auto bytes=c.in+c.ex;c.in=c.ex=0;return bytes;
}
static void domains(){
 heap(46*KiB,27652*KiB);inBlock=29*KiB;dmaBlock=6*KiB;exBlock=27135*KiB;
 Manager m(sample,totals);Cache tiles,worker;tiles.ex=MiB;worker.in=16*KiB;
 check(m.addCache(Owner::Map,"tiles",reclaim,&tiles,{MemoryDomain::Psram}),"register tiles");
 check(m.addCache(Owner::Audio,"idle worker",reclaim,&worker,{MemoryDomain::Internal}),"register worker");
 const Budget git{16*KiB,4*KiB,0,384*KiB,128*KiB};AdmissionReport r;
 auto denied=m.reserve(Owner::Tools,git,&r);check(!denied&&r.reason==Denial::Internal,"reported 46 versus48 denied correctly");
 check(r.memory.psram==27652*KiB&&r.requested.internal==16*KiB,"exact report fixture");
 auto allowed=m.reserveRecovering(Owner::Tools,git,true,&r,100);
 check(bool(allowed)&&r.reason==Denial::None,"reclaim actual SRAM then admit");
 check(tiles.calls==0&&tiles.ex==MiB,"do not discard PSRAM tiles for SRAM deficiency");
 check(worker.calls==1&&m.reclaimedInternal()==16*KiB&&m.reclaimedPsram()==0,"domain-specific measured gain");
 check(InternalSafety==32*KiB&&PsramSafety==512*KiB,"safety margins unchanged");
 allowed.release();
 heap(256*KiB,550*KiB);Manager external(sample,totals);Cache internal,psram;internal.in=KiB;psram.ex=256*KiB;
 external.addCache(Owner::Audio,"internal",reclaim,&internal,{MemoryDomain::Internal});
 external.addCache(Owner::Map,"external",reclaim,&psram,{MemoryDomain::Psram});
 external.collect({0,0,0,128*KiB,0});
 check(internal.calls==0&&psram.calls==1,"PSRAM deficiency skips internal-only resource");
 heap(100*KiB,4*MiB);inBlock=2*KiB;Manager fragmented(sample,totals);Cache map,stack;stack.inLargest=8*KiB;
 fragmented.addCache(Owner::Map,"map",reclaim,&map,{MemoryDomain::Psram});
 fragmented.addCache(Owner::Tools,"stack",reclaim,&stack,{MemoryDomain::Internal});
 fragmented.collect({0,4*KiB,0,0,0});check(map.calls==0&&stack.calls==1,"fragmentation selects internal domain");
 heap();dmaBlock=KiB;Manager dma(sample,totals);Cache dmaCache;dmaCache.dmaLargest=8*KiB;
 dma.addCache(Owner::Audio,"dma",reclaim,&dmaCache,{MemoryDomain::Internal});
 dma.collect({0,0,4*KiB,0,0});check(dmaCache.calls==1,"DMA deficiency belongs to internal domain");
 heap(40*KiB,600*KiB);Manager both(sample,totals);Cache a,b;a.in=32*KiB;b.ex=MiB;
 both.addCache(Owner::Audio,"in",reclaim,&a,{MemoryDomain::Internal});both.addCache(Owner::Map,"ex",reclaim,&b,{MemoryDomain::Psram});
 both.collect({16*KiB,0,0,128*KiB,0});check(a.calls==1&&b.calls==1,"both deficient domains reclaimed");
 heap(46*KiB);Manager pinned(sample,totals);Cache veto;veto.live=true;veto.in=16*KiB;
 pinned.addCache(Owner::Audio,"live playback",reclaim,&veto,{MemoryDomain::Internal});pinned.collect(git);
 check(veto.calls==1&&inFree==46*KiB,"active callback veto protects workload");
 auto refusal=pinned.reserve(Owner::Tools,git);check(!refusal,"no falsely reclaimed bytes");
}
static void ordering(){
 heap(30*KiB);Manager m(sample,totals);std::vector<int> order;Cache f,r,c;
 f.id=1;r.id=2;c.id=3;f.order=r.order=c.order=&order;c.in=32*KiB;
 m.addCache(Owner::Files,"foreground",reclaim,&f);m.addCache(Owner::Tools,"requester",reclaim,&r);m.addCache(Owner::Map,"background",reclaim,&c);
 m.foreground(Owner::Files);m.collect({16*KiB,0,0,0,0},Owner::Tools);
 check(order==std::vector<int>{3},"background resource before requester or foreground");
 heap(40*KiB);Manager nested(sample,totals);Cache n;n.in=16*KiB;n.manager=&nested;nested.addCache(Owner::Map,"nested",reclaim,&n);
 nested.collect({16*KiB,0,0,0,0});check(n.calls==1&&nested.collections()==1,"reentrant callback cannot recurse collector");
}
static void ownershipAndQuiescence(){
 heap(256*KiB,550*KiB);Manager m(sample,totals);Cache visible,requester,hidden;
 visible.ex=requester.ex=MiB;hidden.ex=8*KiB;
 m.addCache(Owner::Map,"visible",reclaim,&visible,{MemoryDomain::Psram});
 m.addCache(Owner::Tools,"requester",reclaim,&requester,{MemoryDomain::Psram});
 m.addCache(Owner::Files,"hidden",reclaim,&hidden,{MemoryDomain::Psram});
 m.foreground(Owner::Map);
 m.collect({0,0,0,128*KiB,0},Owner::Tools);
 check(hidden.calls==1,"background disposable cache attempted under exact demand");
 check(visible.calls==0&&requester.calls==0,"OS protects visible and requesting owner even with unsafe callbacks");
 auto denied=m.reserve(Owner::Tools,{0,0,0,128*KiB,0});check(!denied,"admission fails safely when only active data could satisfy it");
 for(unsigned tick=0;tick<400;++tick)m.serviceCollections();
 check(visible.calls==0&&requester.calls==0&&hidden.calls==1,"final refusal causes no asynchronous generic purge");
 heap(46*KiB);Manager calm(sample,totals);Cache cached;cached.in=16*KiB;cached.ex=MiB;
 calm.addCache(Owner::Files,"retained",reclaim,&cached,{MemoryDomain::Any});
 for(unsigned cycle=0;cycle<400;++cycle){
   calm.foreground(Owner::Files);calm.serviceCollections();
   calm.foreground(Owner::Map);calm.serviceCollections();
   check(cached.calls==0,"navigation and collector service do not expire caches");
 }
 auto admitted=calm.reserveRecovering(Owner::Tools,{16*KiB,0,0,0,0},true,nullptr,0);
 check(bool(admitted)&&cached.calls==1,"real launch reclaims once at operation boundary");
 heap();Manager enough(sample,totals);Cache unused;unused.ex=MiB;
 enough.addCache(Owner::Map,"unused",reclaim,&unused,{MemoryDomain::Psram});
 const auto before=enough.collections();auto sufficient=enough.reserveRecovering(Owner::Tools,{0,0,0,128*KiB,0},true,nullptr,0);
 check(bool(sufficient)&&unused.calls==0&&enough.collections()==before,"successful admission does not collect");
}
static void pendingAndRecovery(){
 heap(100*KiB);Manager m(sample,totals);auto session=m.reserve(Owner::Tools,{16*KiB,0,0,384*KiB,0});check(bool(session),"session reserves headroom");
 auto worker=m.reserve(Owner::Tools,TaskBudget(40*KiB));check(bool(worker),"stack reserve alongside session");inFree.fetch_sub(40*KiB);worker.allocated(40*KiB,0);worker.release();
 auto extra=m.reserve(Owner::Map,{16*KiB,0,0,0,0});check(!extra,"retained session blocks conflicting admission");session.release();
 extra=m.reserve(Owner::Map,{16*KiB,0,0,0,0});check(bool(extra),"reservation release restores access without fake heap gain");extra.release();
 heap(46*KiB);Manager q(sample,totals);Cache c;c.in=16*KiB;q.addCache(Owner::Audio,"worker",reclaim,&c,{MemoryDomain::Internal});
 std::atomic<bool> started{false},finished{false};bool admitted=false;AdmissionReport report;
 std::thread t([&]{started=true;auto r=q.reserveRecovering(Owner::Tools,{16*KiB,0,0,0,0},false,&report,300);admitted=bool(r);finished=true;});
 while(!started)std::this_thread::yield();
 for(unsigned i=0;i<500&&!finished;++i){q.serviceCollections();std::this_thread::sleep_for(std::chrono::milliseconds(1));}t.join();
 check(admitted&&c.calls==1,"worker exact request recovered in operation");check(c.calledOn==std::this_thread::get_id(),"cache destructors run only on collector thread");
 heap(46*KiB);Manager timeout(sample,totals);Cache untouched;timeout.addCache(Owner::Audio,"timeout",reclaim,&untouched);
 std::thread noUi([&]{auto r=timeout.reserveRecovering(Owner::Tools,{16*KiB,0,0,0,0},false,&report,5);check(!r,"unserviced request times out safely");});noUi.join();
 timeout.serviceCollections();check(untouched.calls==0,"timed-out pending request removed");check(timeout.denied()==1,"one denial per recovery, no retry spam");
 heap(46*KiB);Manager deferred(sample,totals);Cache signal;std::atomic<bool> retire{false};signal.notify=&retire;
 deferred.addCache(Owner::Audio,"retired stack",reclaim,&signal,{MemoryDomain::Internal});
 std::thread idle([&]{while(!retire.load(std::memory_order_acquire))std::this_thread::yield();std::this_thread::sleep_for(std::chrono::milliseconds(3));inFree.fetch_add(16*KiB);});
 auto r=deferred.reserveRecovering(Owner::Tools,{16*KiB,0,0,0,0},true,&report,100);idle.join();
 check(bool(r)&&signal.calls==1,"bounded UI yield waits for real deferred stack release");check(deferred.reclaimedInternal()==0,"no predicted reclaimed stack bytes counted");
}
static void cycles(){
 for(unsigned cycle=0;cycle<400;++cycle){
  heap(46*KiB);Manager m(sample,totals);Cache in,ex;in.in=16*KiB;ex.ex=MiB;
  m.addCache(Owner::Map,"map",reclaim,&ex,{MemoryDomain::Psram});m.addCache(Owner::Audio,"idle",reclaim,&in,{MemoryDomain::Internal});
  auto a=m.reserveRecovering(Owner::Tools,{16*KiB,0,0,0,0},true,nullptr,10);check(bool(a),"cycle admission");
  auto moved=std::move(a);check(!a&&bool(moved),"single reservation owner after move");
  inFree.fetch_sub(16*KiB);moved.allocated(16*KiB,0);moved.release();inFree.fetch_add(16*KiB);
  auto b=m.reserve(Owner::Tools,{16*KiB,0,0,0,0});check(bool(b),"cycle no leaked pending budget");
  check(ex.calls==0&&ex.ex==MiB&&in.calls==1,"cycle no external cache churn");
 }
}
int main(){domains();ordering();ownershipAndQuiescence();pendingAndRecovery();cycles();printf("{\"passed\":true,\"checks\":%u,\"cycles\":400}\n",checks);}
