#include <Arduino.h>
#include <iostream>
#include <cstdlib>
#include <memory>
#include "Tab5NetworkGate.h"
std::atomic<unsigned> checks{0},delayCalls{0},takes{0},gives{0};
std::function<void()> afterTake;
TestSerial Tab5DebugSerial;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
class Tab5AudioEngine{public:static void established(Tab5NetworkLease& l){
#ifdef CURRENT_GATE
 l.shareAfterSetup();
#else
 (void)l;
#endif
}static bool transition(unsigned ms){Tab5NetworkTransition t("test-transition",ms);return bool(t);}};
static bool cancelled(void*p){return static_cast<std::atomic<bool>*>(p)->load();}
static void waitFlag(std::atomic<bool>&flag){auto start=millis();while(!flag&&millis()-start<2000)std::this_thread::yield();CHECK(flag);}
int main(){
 {Tab5NetworkLease l("before-boot",0);CHECK(!l);}
 tab5NetworkGateBegin();tab5NetworkGateBegin();
 tab5NetworkSetEnabled(false);{Tab5NetworkLease l("disabled",100);CHECK(!l&&!tab5NetworkBusy());}
 tab5NetworkSetEnabled(true);
 unsigned during=0;{
  Tab5NetworkLease radio("radio",100);CHECK(radio);tab5NetworkSetMediaPending(true);
  CHECK(tab5NetworkBusy()&&tab5NetworkBackgroundYieldRequested());
  Tab5AudioEngine::established(radio);
#ifdef CURRENT_GATE
  tab5NetworkSetMediaPending(false);CHECK(radio&&tab5NetworkBusy()&&!tab5NetworkBackgroundYieldRequested());
#endif
  std::thread tile([&]{Tab5NetworkLease l("map",30,nullptr,nullptr,true);during=bool(l);});tile.join();
#ifdef CURRENT_GATE
  CHECK(during==1);
#else
  CHECK(during==0);
#endif
 }
 tab5NetworkSetMediaPending(false);CHECK(!tab5NetworkBusy()&&!tab5NetworkBackgroundYieldRequested());
 {Tab5NetworkLease l("after-pause",0,nullptr,nullptr,true);CHECK(l);} // Radio destructor closes its session.
 // A pending cancellation never grants ownership or unlocks another client.
 {Tab5NetworkLease active("active",100);std::atomic<bool> stop{false};std::atomic<bool> entered{false};bool got=true;std::thread waiter([&]{entered=true;Tab5NetworkLease l("cancelled",500,cancelled,&stop);got=bool(l);});waitFlag(entered);stop=true;waiter.join();CHECK(!got&&tab5NetworkBusy());}
 CHECK(!tab5NetworkBusy()&&!tab5NetworkBackgroundYieldRequested());
 {std::atomic<bool> stop{true};auto n=takes.load();Tab5NetworkLease l("already-cancelled",100,cancelled,&stop);CHECK(!l&&takes==n);}
 // Disabling admission while a waiter sleeps must remain effective after lock acquisition.
 {Tab5NetworkLease active("active",100);std::atomic<bool> entered{false};bool got=true;std::thread waiter([&]{entered=true;Tab5NetworkLease l("disabled-waiter",100);got=bool(l);});waitFlag(entered);tab5NetworkSetEnabled(false);waiter.join();CHECK(!got);}
 CHECK(!tab5NetworkBusy()&&!tab5NetworkBackgroundYieldRequested());tab5NetworkSetEnabled(true);
 // Retained sockets wake their waiter when closed; no millisecond spin is needed.
 {tab5NetworkSetBackgroundRetained(true);std::atomic<bool> entered{false},got{false};std::thread waiter([&]{entered=true;Tab5NetworkLease l("retained",500);got=bool(l);});waitFlag(entered);std::this_thread::sleep_for(std::chrono::milliseconds(25));CHECK(!got);tab5NetworkSetBackgroundRetained(false);waiter.join();CHECK(got);}
 unsigned raceAdmitted=0;
 // Inject the retained connection appearing after the precheck but while taking the mutex.
 {std::atomic<bool> injected{false};afterTake=[&]{afterTake={};tab5NetworkSetBackgroundRetained(true);injected=true;};std::atomic<bool> done{false};std::thread waiter([&]{Tab5NetworkLease l("handover-race",40);raceAdmitted=bool(l);done=true;});waitFlag(injected);waiter.join();CHECK(done);tab5NetworkSetBackgroundRetained(false);}
#ifdef CURRENT_GATE
 CHECK(raceAdmitted==0);CHECK(delayCalls==0);
 // Closing the shared radio while another request owns the gate cannot give that mutex.
 {auto radio=std::make_unique<Tab5NetworkLease>("radio",100);CHECK(*radio);Tab5AudioEngine::established(*radio);
  std::atomic<bool> entered{false},release{false};std::thread map([&]{Tab5NetworkLease l("map",100,nullptr,nullptr,true);CHECK(l);entered=true;waitFlag(release);});waitFlag(entered);const auto n=gives.load();radio.reset();CHECK(tab5NetworkBusy()&&gives==n);release=true;map.join();CHECK(!tab5NetworkBusy());}
 // Driver changes cannot proceed while the shared radio owns its live transport.
 {auto radio=std::make_unique<Tab5NetworkLease>("radio",100);CHECK(*radio);Tab5AudioEngine::established(*radio);tab5NetworkSetEnabled(false);
  bool granted=true;std::thread blocked([&]{granted=Tab5AudioEngine::transition(35);});blocked.join();CHECK(!granted&&tab5NetworkBusy());
  std::atomic<bool> entered{false},transitionDone{false};std::thread waiter([&]{entered=true;transitionDone=Tab5AudioEngine::transition(500);});waitFlag(entered);std::this_thread::sleep_for(std::chrono::milliseconds(25));CHECK(!transitionDone);radio.reset();waiter.join();CHECK(transitionDone&&!tab5NetworkBusy());tab5NetworkSetEnabled(true);}
 // Repeated coexistence remains bounded: one established radio + one request,
 // including simultaneous callers and radio close/reopen cycles.
 unsigned maxOther=0;
 for(unsigned round=0;round<100;++round){Tab5NetworkLease radio("radio",100);CHECK(radio);Tab5AudioEngine::established(radio);std::atomic<unsigned> concurrent{0};auto request=[&]{for(unsigned j=0;j<4;++j){Tab5NetworkLease l("other",500,nullptr,nullptr,true);CHECK(l);auto c=++concurrent;CHECK(c==1);maxOther=std::max(maxOther,c);std::this_thread::yield();--concurrent;}};std::thread a(request),b(request);a.join();b.join();CHECK(concurrent==0&&tab5NetworkBusy());}
 CHECK(maxOther==1&&!tab5NetworkBusy()&&!tab5NetworkBackgroundYieldRequested());
#else
 CHECK(raceAdmitted==1);
#endif
 CHECK(takes>=gives);
 std::cout<<"checks="<<checks<<" PASS; radio/map admitted="<<during<<"; retained-handover unsafe admission="<<raceAdmitted<<"; legacy delay calls="<<delayCalls<<"; live leases=0; mutex ownership checked\n";
}
