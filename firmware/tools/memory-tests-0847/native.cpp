#include "Tab5ResourceManager.h"
#include <freertos/event_groups.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include <functional>
using namespace Tab5Resources;
std::atomic<unsigned> delays{0},takes{0},gives{0},mutexCreates{0},eventWaits{0},eventSets{0},eventCreates{0};
std::function<void()> beforeEventWait;
bool tab5TlsMemoryExternal(){return true;}
static std::atomic<unsigned> checks{0};
static std::atomic<size_t> available{0};
static std::function<void()> sampleHook;
static void check(bool ok,const char* why){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",why);abort();}}
static MemorySample sample(){if(sampleHook)sampleHook();const size_t n=available.load();return {n,n,n,16*MiB,16*MiB};}
static MemoryTotals totals(){return {available.load(),16*MiB};}
static MemorySample requested(const Budget&){return sample();}
static void begin(Manager& m){
#if TAB5_TEST_FIXED
 m.begin();m.begin();
#else
 (void)m;
#endif
}
static void reset(){available=30*KiB;delays=eventWaits=eventSets=0;sampleHook={};beforeEventWait={};}
static constexpr Budget stack{16*KiB,16*KiB,0,0,0};
static void until(const std::atomic<bool>& done){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!done.load()){if(std::chrono::steady_clock::now()>=end)check(false,"test barrier completed");std::this_thread::yield();}}
struct Cache{std::atomic<unsigned> calls{0};std::atomic<bool> entered{false},hold{false};std::thread::id thread;size_t bytes=200*KiB;};
static size_t reclaim(void* p){auto& c=*static_cast<Cache*>(p);c.thread=std::this_thread::get_id();++c.calls;c.entered=true;while(c.hold)std::this_thread::yield();available=c.bytes;return c.bytes;}
static unsigned delayedCollector(){reset();Manager m(sample,totals,requested);begin(m);Cache c;m.addCache(Owner::Files,"reloadable",reclaim,&c);m.foreground(Owner::Audio);std::atomic<bool> started{false},done{false};AdmissionReport report;bool accepted=false;
 std::thread worker([&]{started=true;auto q=m.reserveRecovering(Owner::Audio,stack,false,&report,500);accepted=bool(q);done=true;});until(started);std::this_thread::sleep_for(std::chrono::milliseconds(40));
 const unsigned sleepingPolls=delays.load();m.serviceCollections();worker.join();check(accepted&&report.reason==Denial::None,"pending collection admitted after completion");check(c.calls==1&&c.thread==std::this_thread::get_id(),"cache reclaimed on collector only");check(m.denied()==0,"success does not count refusals");check(InternalSafety==32*KiB&&PsramSafety==512*KiB,"safety margins unchanged");
#if TAB5_TEST_FIXED
 check(sleepingPolls==0&&eventWaits==1,"one native completion wait replaces pending tick polling");
#else
 check(sleepingPolls>10,"baseline pending request wakes every tick");
#endif
 return sleepingPolls;
}
static void noCollectorTimeout(){reset();Manager m(sample,totals,requested);begin(m);Cache c;m.addCache(Owner::Files,"reloadable",reclaim,&c);AdmissionReport report;std::thread worker([&]{auto q=m.reserveRecovering(Owner::Audio,stack,false,&report,20);check(!q&&report.reason==Denial::Internal,"unserviced timeout preserves actual final refusal");});worker.join();m.serviceCollections();check(!c.calls&&m.denied()==1,"expired pending callback removed exactly once");}
static void abandonedCallback(){reset();Manager m(sample,totals,requested);begin(m);Cache c;c.hold=true;m.addCache(Owner::Files,"reloadable",reclaim,&c);std::atomic<bool> started{false};AdmissionReport r;
 std::thread worker([&]{started=true;auto q=m.reserveRecovering(Owner::Audio,stack,false,&r,40);check(!q&&r.reason==Denial::CollectionBusy,"in-flight callback retains ownership beyond waiter timeout");});until(started);
 std::thread collector([&]{while(!c.entered){m.serviceCollections();std::this_thread::yield();}});until(c.entered);worker.join();check(c.calls==1,"abandoned callback executes once");c.hold=false;collector.join();m.serviceCollections();auto q=m.reserve(Owner::Audio,stack);check(bool(q),"slot safely reusable after abandoned collector exits");}
static void simultaneousRequests(){reset();Manager m(sample,totals,requested);begin(m);Cache c;c.bytes=256*KiB;m.addCache(Owner::Files,"reloadable",reclaim,&c);std::atomic<unsigned> entered{0},accepted{0};std::atomic<bool> release{false};std::vector<std::thread> workers;
 for(unsigned i=0;i<8;++i)workers.emplace_back([&]{++entered;auto q=m.reserveRecovering(Owner::Audio,stack,false,nullptr,500);if(q)++accepted;while(!release)std::this_thread::yield();});
 while(entered<8)std::this_thread::yield();std::this_thread::sleep_for(std::chrono::milliseconds(20));m.serviceCollections();
 const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(accepted<8&&std::chrono::steady_clock::now()<end){m.serviceCollections();std::this_thread::yield();}
 release=true;for(auto& t:workers)t.join();check(accepted==8,"each waiter receives its own completion");check(c.calls==1,"single deficit reclamation suffices without duplicate cache destruction");check(m.denied()==0,"concurrent successful requests not rejected");}
static void contention(){reset();available=256*KiB;Manager m(sample,totals,requested);begin(m);std::atomic<bool> entered{false},release{false};std::atomic<unsigned> calls{0};sampleHook=[&]{if(calls.fetch_add(1)==0){entered=true;while(!release)std::this_thread::yield();}};bool first=false;std::thread holder([&]{auto q=m.reserve(Owner::Audio,stack);first=bool(q);});until(entered);AdmissionReport r;auto q=m.reserve(Owner::Tools,stack,&r);check(!q&&r.reason==Denial::AdmissionBusy,"bounded lock contention is not misreported as exhausted RAM");release=true;holder.join();check(first,"holder completes intact");
#if TAB5_TEST_FIXED
 check(delays==0,"native priority-inheriting mutex has no tick polling");
#else
 check(delays==8,"baseline admission polls eight ticks");
#endif
 sampleHook={};}
#if TAB5_TEST_FIXED
static void completeBeforeSleep(){reset();Manager m(sample,totals,requested);begin(m);Cache c;m.addCache(Owner::Files,"reloadable",reclaim,&c);std::atomic<bool> beforeWait{false},allowWait{false};beforeEventWait=[&]{beforeWait=true;while(!allowWait)std::this_thread::yield();};bool accepted=false;std::thread worker([&]{auto q=m.reserveRecovering(Owner::Audio,stack,false,nullptr,500);accepted=bool(q);});until(beforeWait);m.serviceCollections();check(c.calls==1,"collector finished before native wait");allowWait=true;worker.join();check(accepted&&eventWaits==1,"completion retained when it arrives before wait");beforeEventWait={};}
#endif
int main(){const unsigned polling=delayedCollector();noCollectorTimeout();abandonedCallback();simultaneousRequests();contention();
#if TAB5_TEST_FIXED
 completeBeforeSleep();check(mutexCreates==6&&eventCreates==6,"idempotent boot initialization uses one pair of static objects per manager");
#endif
 printf("{\"passed\":true,\"checks\":%u,\"pendingCollectorTickWakeups\":%u,\"nativeMutexCreates\":%u,\"nativeEventCreates\":%u}\n",checks.load(),polling,mutexCreates.load(),eventCreates.load());}
