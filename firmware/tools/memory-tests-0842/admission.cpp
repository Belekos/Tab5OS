#include "Tab5ResourceManager.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
using namespace Tab5Resources;
static unsigned checks;
static void check(bool ok,const char* why){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",why);abort();}}
static std::atomic<size_t> inFree{0},inBlock{0},dmaBlock{0},exFree{0},exBlock{0};
static std::atomic<unsigned> scans{0},counters{0},inScans{0},dmaScans{0},exScans{0};
static MemorySample all(){++scans;++inScans;++dmaScans;++exScans;return {inFree,inBlock,dmaBlock,exFree,exBlock};}
static MemoryTotals totals(){++counters;return {inFree,exFree};}
static MemorySample requested(const Budget& b){MemorySample m;m.internal=inFree;m.psram=exFree;if(b.internalBlock){++inScans;m.internalBlock=inBlock;}if(b.dmaBlock){++dmaScans;m.dmaBlock=dmaBlock;}if(b.psramBlock){++exScans;m.psramBlock=exBlock;}return m;}
static void heap(size_t internal=30*KiB,size_t external=8*MiB){inFree=internal;inBlock=internal;dmaBlock=internal;exFree=external;exBlock=external;scans=counters=inScans=dmaScans=exScans=0;}
static Manager manager(){
#if TAB5_TEST_FIXED
  return Manager(all,totals,requested);
#else
  return Manager(all,totals);
#endif
}
static constexpr Budget stack{16*KiB,12*KiB,0,0,0};
static unsigned noBytes(){heap();auto m=manager();AdmissionReport r;auto reservation=m.reserveRecovering(Owner::Tools,stack,true,&r,20);
  check(!reservation&&r.reason==Denial::Internal,"low internal bytes refused");
  check(r.memory.internal==30*KiB&&r.memory.internalBlock==30*KiB&&r.memory.dmaBlock==30*KiB&&r.memory.psram==8*MiB&&r.memory.psramBlock==8*MiB,"final refusal remains a full real sample");
  check(m.denied()==1,"one final denial, no retry inflation");
  const unsigned walked=inScans+dmaScans+exScans;
#if TAB5_TEST_FIXED
  check(scans==1&&exScans==1&&dmaScans==1,"recovery byte deficit never repeatedly walks full heaps");
  check(counters>3,"byte counters rechecked while awaiting released stack");
#else
  check(scans>3,"baseline exhibits repeated complete heap walks");
#endif
  return walked;
}
static void successful(){heap(160*KiB);auto m=manager();AdmissionReport r;auto reservation=m.reserveRecovering(Owner::Tools,stack,true,&r,10);
  check(bool(reservation)&&r.reason==Denial::None,"enough internal memory admitted");
  check(inScans==1,"required stack block checked");
#if TAB5_TEST_FIXED
  check(dmaScans==0&&exScans==0&&scans==0,"stack-only admission does not traverse DMA or PSRAM");
#endif
  reservation.release();
  const Budget both{16*KiB,12*KiB,8*KiB,1024*KiB,512*KiB};
  auto b=m.reserveRecovering(Owner::Image,both,true,&r,10);
  check(bool(b)&&dmaScans>0&&exScans>0,"requested DMA and PSRAM block checks retained");
}
static void fragmentation(){
  for(unsigned domain=0;domain<3;++domain){heap(160*KiB);auto m=manager();AdmissionReport r;const Budget b{16*KiB,12*KiB,8*KiB,1024*KiB,512*KiB};
    if(domain==0)inBlock=4*KiB;if(domain==1)dmaBlock=4*KiB;if(domain==2)exBlock=256*KiB;
    auto q=m.reserveRecovering(Owner::Tools,b,true,&r,0);
    check(!q,"fragmentation not hidden by free-byte precheck");
    check(r.reason==(domain==0?Denial::InternalBlock:domain==1?Denial::DmaBlock:Denial::PsramBlock),"original reason order and capability retained");
  }
}
static void pendingAndRelease(){heap(80*KiB);auto m=manager();auto held=m.reserve(Owner::Audio,{20*KiB,0,0,0,0});check(bool(held),"existing quota admitted");
  AdmissionReport r;const Budget large{32*KiB,12*KiB,0,0,0};auto fail=m.reserveRecovering(Owner::Tools,large,true,&r,2);check(!fail&&r.pending.internal==20*KiB,"byte precheck includes live reservations");held.release();auto ok=m.reserveRecovering(Owner::Tools,large,true,&r,10);check(bool(ok),"released quota immediately permits admission");
}
static void deferred(){heap();auto m=manager();std::thread retired([]{std::this_thread::sleep_for(std::chrono::milliseconds(3));inBlock=70*KiB;inFree=70*KiB;});AdmissionReport r;auto q=m.reserveRecovering(Owner::Tools,stack,true,&r,50);retired.join();check(bool(q)&&r.reason==Denial::None,"real idle stack reclamation observed without a new delay");check(inScans>0,"admission after free counter increase verifies the block");}
static void worker(){heap();auto m=manager();std::atomic<bool> done{false};AdmissionReport r;bool accepted=true;
  std::thread job([&]{auto q=m.reserveRecovering(Owner::Tools,stack,false,&r,30);accepted=bool(q);done=true;});
  while(!done){m.serviceCollections();std::this_thread::sleep_for(std::chrono::milliseconds(1));}job.join();
  check(!accepted&&r.reason==Denial::Internal&&m.denied()==1,"worker recovery bounded with exact final refusal");
#if TAB5_TEST_FIXED
  check(scans==1,"worker byte deficit also avoids repetitive complete walks");
#endif
  m.serviceCollections();
}
int main(){const auto baselineWalks=noBytes();successful();fragmentation();pendingAndRelease();deferred();worker();check(InternalSafety==32*KiB&&PsramSafety==512*KiB,"all safety margins retained");printf("{\"passed\":true,\"checks\":%u,\"stableInternalDeficitHeapWalks\":%u}\n",checks,baselineWalks);}
