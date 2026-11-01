#include "Tab5ResourceManager.h"
#include <freertos/event_groups.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>
using namespace Tab5Resources;
std::atomic<unsigned> delays{0},takes{0},gives{0},mutexCreates{0},eventWaits{0},eventSets{0},eventCreates{0};
std::function<void()> beforeEventWait,beforeEventSet;
bool tab5TlsMemoryExternal(){return true;}
static std::atomic<size_t> available{30*KiB};static unsigned checks=0;
static void check(bool value,const char* reason){++checks;if(!value){fprintf(stderr,"FAIL %s\n",reason);std::abort();}}
static MemorySample sample(){auto n=available.load();return {n,n,n,16*MiB,16*MiB};}
static MemoryTotals totals(){return {available.load(),16*MiB};}
static MemorySample requested(const Budget&){return sample();}
static size_t reclaim(void*){available=200*KiB;return 170*KiB;}
template<class F>static void until(F ready){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!ready()){if(std::chrono::steady_clock::now()>deadline)check(false,"test barrier timeout");std::this_thread::yield();}}
int main(){
 constexpr Budget budget{16*KiB,16*KiB,0,0,0};
 for(unsigned cycle=0;cycle<100;++cycle){
  available=30*KiB;eventWaits=0;Manager manager(sample,totals,requested);manager.begin();manager.addCache(Owner::Files,"test",reclaim,nullptr);manager.foreground(Owner::Audio);
  std::atomic<unsigned> waits{0},sets{0};std::atomic<bool> setBlocked{false},allowSet{false},doneB{false};bool acceptedA=false,acceptedB=false;
  beforeEventWait=[&]{++waits;};
  beforeEventSet=[&]{if(sets.fetch_add(1)==0){setBlocked=true;while(!allowSet)std::this_thread::yield();}};
  std::thread workerA([&]{auto r=manager.reserveRecovering(Owner::Audio,budget,false,nullptr,20);acceptedA=bool(r);});
  until([&]{return waits.load()>=1;});
  std::thread collector([&]{manager.serviceCollections();});until([&]{return setBlocked.load();});
  // Done has been published, but its event bit has not. The timeout path
  // safely releases that slot because the callback itself already returned.
  workerA.join();check(acceptedA,"done request can complete before event publication");
  available=30*KiB;
  std::thread workerB([&]{auto r=manager.reserveRecovering(Owner::Audio,budget,false,nullptr,1000);acceptedB=bool(r);doneB=true;});
  until([&]{return waits.load()>=2;});
  available=200*KiB;allowSet=true;collector.join();
  until([&]{return waits.load()>=3||doneB.load();});
  check(!doneB,"old completion bit cannot admit reused pending slot");
  manager.serviceCollections();workerB.join();check(acceptedB,"new request admits after its own collection completes");
  check(manager.denied()==0,"valid slot reuse has no false refusals");
  beforeEventWait={};beforeEventSet={};
 }
 printf("PASS %u independent checks across100 Done-before-event / slot-reuse / stale-event races.\n",checks);
}
