#include "Tab5BatteryGuard.h"
#include "M5Unified.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initializer_list>
using namespace Tab5BatteryGuard;
FakeM5 M5;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Sample discharge(int mv = 6800) { Sample s; s.valid=true; s.millivolts=mv; s.milliamps=-300; return s; }
static void policyTests() {
  { Policy p; Result r; for(uint32_t t=0;t<40000;t+=1000) {r=p.update(t,discharge()); CHECK(r.event!=Event::Shutdown);}
    CHECK(p.update(40000,discharge()).event==Event::Shutdown); }
  { Policy p; for(uint32_t t=0;t<8000;t+=1000)CHECK(p.update(t,discharge(6400)).event!=Event::Shutdown);
    CHECK(p.update(8000,discharge(6400)).event==Event::Shutdown); }
  { Policy p; for(uint32_t t=0;t<15000;t+=1000)CHECK(p.update(t,discharge(7000)).event!=Event::Low);
    CHECK(p.update(15000,discharge(7000)).event==Event::Low);
    for(uint32_t t=16000;t<30000;t+=1000)CHECK(p.update(t,discharge(7000)).event==Event::None); }
  { Policy p; for(uint32_t t=0;t<=10000;t+=1000)p.update(t,discharge());
    CHECK(p.update(11000,discharge(6900)).countdown);
    CHECK(p.update(12000,discharge(6950)).event==Event::Cancelled); CHECK(!p.ready()); }
  { Policy p; for(uint32_t t=0;t<=10000;t+=1000)p.update(t,discharge());
    CHECK(p.update(18000,discharge()).event==Event::Cancelled); CHECK(!p.ready());
    CHECK(p.update(19000,discharge()).event==Event::None); }
  { Policy p; for(uint32_t t=0;t<=10000;t+=1000)p.update(t,discharge());
    Sample invalid; CHECK(p.update(11000,invalid).event==Event::Cancelled); CHECK(!p.ready()); }
  { Policy p; Sample s=discharge(6400); s.charging=true; p.update(0,s); s.charging=false;
    for(uint32_t t=1000;t<30000;t+=1000) CHECK(!p.update(t,s).countdown);
    for(uint32_t t=30000;t<38000;t+=1000) CHECK(p.update(t,s).event!=Event::Shutdown);
    CHECK(p.update(38000,s).event==Event::Shutdown); }
  { Policy p; Sample s=discharge(6400); s.milliamps=200; p.update(0,s); s.milliamps=-300;
    for(uint32_t t=1000;t<30000;t+=1000)CHECK(!p.update(t,s).countdown); }
  { Policy p; for(uint32_t t=0;t<=10000;t+=1000)p.update(t,discharge());
    for(uint32_t t=11000;t<=14000;t+=1000)p.update(t,discharge(6300));
    CHECK(p.update(15000,discharge(6300)).seconds==4);
    for(uint32_t t=16000;t<19000;t+=1000)CHECK(p.update(t,discharge(6300)).event!=Event::Shutdown);
    CHECK(p.update(19000,discharge(6300)).event==Event::Shutdown); }
  // millis wrap: durations and deadline use unsigned elapsed / bounded signed delta.
  { Policy p; const uint32_t base=0xfffff000u;
    for(uint32_t t=0;t<40000;t+=1000)CHECK(p.update(base+t,discharge()).event!=Event::Shutdown);
    CHECK(p.update(base+40000,discharge()).event==Event::Shutdown); }
  // Every invalid/absent/charging/neutral sample must cancel, including at deadline.
  for(int mv: {0, 1, 1040, 4400, 4500, 6000, 6800, 7000, 8800, 9000, 32767}) {
    for(int ma: {-9000, -300, -24, 0, 100, 9000}) for(bool valid: {false,true}) for(bool charge: {false,true}) {
      Sample s; s.valid=valid;s.charging=charge;s.millivolts=mv;s.milliamps=ma;
      if(Policy::discharging(s)&&mv<Policy::recoverMv)continue;
      Policy p; for(uint32_t t=0;t<40000;t+=1000)p.update(t,discharge());
      CHECK(p.update(40000,s).event==Event::Cancelled);CHECK(!p.ready());
    }
  }
}
static void adapterTest(int scenario) {
  if(scenario<5)M5.In_I2C.fail=scenario;
  if(scenario==5)M5.In_I2C.id=0;
  if(scenario==6)M5.In_I2C.config=0x4120;
  if(scenario==7)M5.board=m5::board_t::other;
  if(scenario==8)M5.In_I2C.mv=0;
  if(scenario==9)M5.In_I2C.ma=0;
  if(scenario==10)M5.In_I2C.charging=true;
  if(scenario==11)M5.In_I2C.ma=300;
  // User's observed recovery state: 1.04 V / +0 mA is not a draining 2S pack.
  if(scenario==17){M5.In_I2C.mv=1040;M5.In_I2C.ma=0;}
  const bool mustNotStop=scenario<12||scenario==17;
  Result r;
  for(uint32_t t=0;t<=40000;t+=1000) {
    r=service(t); int reads=M5.In_I2C.reads;
    CHECK(service(t+1).event==Event::None); CHECK(M5.In_I2C.reads==reads);
    if(mustNotStop) {CHECK(r.event!=Event::Shutdown);CHECK(!powerOffIfStillCritical(t));}
  }
  if(mustNotStop){CHECK(M5.Power.off==0);if(scenario==17)CHECK(!r.sample.valid);}
  else {
    CHECK(r.event==Event::Shutdown);CHECK(r.sample.valid);CHECK(r.sample.millivolts==6800);CHECK(r.sample.milliamps==-300);
    if(scenario==12) {CHECK(powerOffIfStillCritical(40000));CHECK(M5.Power.off==1);}
    else {
      if(scenario==13)M5.In_I2C.charging=true;
      if(scenario==14)M5.In_I2C.fail=4;
      if(scenario==15)M5.In_I2C.ma=0;
      if(scenario==16)M5.In_I2C.mv=7000;
      CHECK(!powerOffIfStillCritical(40000));CHECK(M5.Power.off==0);CHECK(status().event==Event::Cancelled);
    }
  }
}
int main(int argc,char**argv) {
  if(argc==1)policyTests();else adapterTest(atoi(argv[1]));
  printf("PASS %u checks\n",checks);return 0;
}
