#include "Tab5MediaSession.h"
#include "boundary.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
namespace S=Tab5MediaSession;
static unsigned checks=0;
static void check(bool value,const char* reason){++checks;if(!value){std::fprintf(stderr,"FAIL %u %s\n",checks,reason);std::abort();}}
static bool settle(uint32_t now){for(unsigned n=0;n<5;++n){const bool ok=S::flush(now);Boundary::join();if(ok)return true;}return false;}
static void tick(uint32_t now){S::service(now);Boundary::join();S::service(now);Boundary::join();}
static uint32_t checksum(const void* input,size_t n){uint32_t h=2166136261U;auto* p=static_cast<const uint8_t*>(input);while(n--)h=(h^*p++)*16777619U;return h;}
static void clean(uint8_t legacy=150,bool available=false){Boundary::join();check(S::begin(legacy,available),"begin succeeds");}
int main(int argc,char** argv){
 check(argc==2,"output directory argument");
 clean(77,true);
 S::browser(1,2,"/Audio/Classical","FR/news",100);
 S::select(1,"https://radio.example/live","Example",100);
 S::search(1,"Example",100);S::scroll(1,2442,100);
 for(unsigned n=100;n<1200;n+=20)tick(n);
 check(Boundary::writes==0,"debounce before1200ms");
 tick(1200);check(Boundary::writes==1,"legacy volume persisted once before browser quiet");
 tick(1300);check(Boundary::writes==2,"browser record after quiet");
 struct Record{uint32_t magic,version,size,hash;S::State state;};
 Record expected{0x4D353254,1,sizeof(S::State),checksum(&S::state(),sizeof(S::State)),S::state()};
 auto record=Boundary::values.at("browser");
 check(record.size()==sizeof(expected)&&!std::memcmp(record.data(),&expected,sizeof(expected)),"exact compatible version1 record bytes");
 {std::ofstream out(std::string(argv[1])+"/record.bin",std::ios::binary);out.write(reinterpret_cast<const char*>(record.data()),record.size());}
 clean();check(S::volume()==77&&S::state().view==1&&S::state().favorites==2,"restores independent browser and volume");
 check(!std::strcmp(S::state().folder,"/Audio/Classical")&&!std::strcmp(S::state().selected[1],"https://radio.example/live")&&S::state().scroll[1]==2442,"restores identity and scroll");
 auto oldWrites=Boundary::writes,oldLaunches=Boundary::launches;
 for(unsigned n=2000;n<6000;++n){S::browser(1,2,"/Audio/Classical","FR/news",n);S::select(1,"https://radio.example/live","Example",n);S::scroll(1,2442,n);S::service(n);}
 check(Boundary::writes==oldWrites&&Boundary::launches==oldLaunches,"4000 unchanged updates no writes or worker");
 for(unsigned n=0;n<255;++n){S::volume(uint8_t(n),10000+n);S::service(10000+n);}
 check(Boundary::writes==oldWrites,"slider sample coalescing");
 S::volume(0,11000);check(settle(11001),"mute durable after completion");clean();check(S::volume()==0,"mute survives restart");
 S::volume(64,12000);Boundary::writable=false;check(!settle(12001),"failure never reports durability");
 oldWrites=Boundary::attempts;for(unsigned n=12002;n<17001;n+=10)S::service(n);
 check(Boundary::attempts==oldWrites,"retry bounded5seconds");Boundary::writable=true;tick(17001);check(settle(17001),"failure retry completes");clean();check(S::volume()==64,"retry preserved volume");
 oldWrites=Boundary::writes;S::browser(0,0,"/Audio","",20000);S::browser(1,2,"/Audio/Classical","FR/news",20100);tick(21500);
 check(Boundary::writes==oldWrites,"unchanged state hash avoids write");
 S::select(1,std::string(700,'a').c_str(),"oversized",22000);check(!S::state().selected[1][0],"overlong identity not truncated");
 S::scroll(1,-9,22001);check(S::state().scroll[1]==0,"negative scroll clamped");
 S::scroll(1,2000000,22001);check(S::state().scroll[1]==1000000,"oversized scroll clamped");
 check(settle(22002),"boundary data saved");Boundary::values["browser"].back()^=1;
 clean();check(S::state().view==0&&S::volume()==64,"corrupt browser rejected independent volume retained");
 S::volume(81,0xfffffff0U);tick(0x10U);oldWrites=Boundary::writes;tick(0x4a0U);
 check(Boundary::writes==oldWrites+1,"debounce across millis wrap");clean();check(S::volume()==81,"wrapped change persisted");
 Boundary::values.clear();clean(150,false);tick(5000);check(Boundary::values.empty(),"cardless default not silently persisted");
 check(S::seedLegacyVolume(37,5100),"late SD migration accepted");tick(6400);clean();check(S::volume()==37,"late migration durable");
 check(!S::seedLegacyVolume(99,7000)&&S::volume()==37,"established volume wins SD migration");
 Boundary::values.clear();clean(150,false);S::volume(0,10);check(!S::seedLegacyVolume(99,11),"user choice wins late migration");check(settle(12),"cardless chosen volume persisted");

#if CURRENT
 // A worker held at the NVS boundary must not block the UI or borrow live State.
 clean();S::browser(1,0,"/Audio/Old","old",100);S::volume(21,100);
 Boundary::pause();check(!S::flush(101)&&S::saving(),"flush only launches, not a false durable result");Boundary::waitEntered();
 check(!S::begin(99),"begin cannot delete state during a pending write");
 S::browser(2,3,"/Video/New","new",102);S::select(2,"/Video/New/clip.avi","Clip",102);
 std::thread volumeProducer([]{for(unsigned i=0;i<50000;++i)S::volume(uint8_t(i),102+i);S::volume(188,60000);});volumeProducer.join();
 for(unsigned n=0;n<4000;++n){S::service(60000+n);check(S::saving(),"UI responsive while NVS worker blocked");}
 check(!S::flush(65000),"later flush pending does not claim success");
 Boundary::underruns+=2;Boundary::release();Boundary::join();
 check(settle(65001),"later mutations persist after old snapshot");clean();
 check(S::state().view==2&&!std::strcmp(S::state().selected[2],"/Video/New/clip.avi")&&S::volume()==188,"newer browser+volume not erased by old completion");
 check(S::snapshot().underrunsDuringWrites==2&&S::snapshot().maxWriteUs>=5000,"job-only duration and DSI correlation");
 check(Boundary::maxLiveJobs==1&&Boundary::liveJobs==0&&Boundary::reservations==0,"one snapshot and no leftover allocations/reservations");

 // Partial commit of separate NVS keys: retry only the unsuccessful component.
 S::browser(3,1,"/Recordings","",70000);S::volume(41,70000);Boundary::failBrowser=true;
 check(!settle(70001),"browser failure reports incomplete even if volume written");
 check(Boundary::values.at("volume")[0]==41,"successful independent volume preserved");
 oldWrites=Boundary::writes;Boundary::failBrowser=false;tick(75001);check(settle(75001),"browser retries at deadline");
 check(Boundary::writes==oldWrites+1,"already committed volume is not redundantly rewritten");
 S::browser(0,0,"/Audio/Partial","",80000);S::volume(99,80000);Boundary::failVolume=true;
 check(!settle(80001),"volume failure reports incomplete even if browser saved");Boundary::failVolume=false;
 oldWrites=Boundary::writes;tick(85001);check(settle(85001),"volume retry succeeds");
 check(Boundary::writes==oldWrites+1,"already committed browser not redundantly rewritten");

 // Errors before launch also retain pending state and do not allocate every frame.
 for(unsigned failure=0;failure<3;++failure){
  uint32_t now=90000+failure*10000;S::volume(uint8_t(120+failure),now);
  Boundary::failAllocation=failure==0;Boundary::failAdmission=failure==1;Boundary::failTask=failure==2;
  auto launches=Boundary::launches;check(!S::flush(now+1),"allocation/admission/task failure remains dirty");
  for(unsigned i=2;i<5001;i+=31)S::service(now+i);
  check(Boundary::launches==launches&&Boundary::liveJobs==0&&Boundary::reservations==0,"failure cleanup and no retry storm");
  Boundary::failAdmission=false;tick(now+5001);check(settle(now+5001),"deferred job eventually persists");
 }
 // Namespace open failure and retry across uint32 wrap.
 S::volume(213,0xfffffff0U);Boundary::available=false;check(!settle(0xfffffff1U),"NVS open failure retained");
 auto launches=Boundary::launches;for(uint32_t n=1;n<4900;n+=29)S::service(n);
 check(Boundary::launches==launches,"wrapped retry bounded");Boundary::available=true;tick(5000);check(settle(5000),"wrapped failure retry succeeds");
 Boundary::displayObserved=false;S::volume(215,6000);check(settle(6001),"save while DSI observation unavailable");check(!S::snapshot().dsiObserved,"zero count not advertised as observed");Boundary::displayObserved=true;
#endif
 // Repeated live preference mutation/start/finish/restart exercises teardown.
 for(unsigned i=0;i<400;++i){uint32_t now=200000+i*2000;S::volume(uint8_t(i),now);S::browser(i%4,i%4,(i%2)?"/Audio/A":"/Audio/B","",now);check(settle(now+1),"cycle persisted");clean();check(S::volume()==uint8_t(i)&&S::state().view==i%4,"cycle survives restart");}
 check(Boundary::liveJobs==0&&Boundary::reservations==0,"job allocations and admissions returned");
#if CURRENT
 check(Boundary::uiWrites==0,"no putBytes runs on UI thread");check(!S::saving(),"no pending task after400 cycles");
#else
 check(Boundary::uiWrites==Boundary::attempts&&Boundary::uiWrites>0,"baseline reproduces synchronous UI writes");
#endif
 std::printf("{\"passed\":true,\"checks\":%u,\"current\":%s,\"writes\":%u,\"attempts\":%u,\"uiWrites\":%u,\"jobLaunches\":%u,\"jobHeapRemaining\":%u,\"maxConcurrentJobs\":%u,\"reservationsRemaining\":%u,\"recordBytes\":%zu,\"taskStackBytes\":%u,\"cycles\":400,\"idleUpdates\":4000}\n",checks,CURRENT?"true":"false",Boundary::writes,Boundary::attempts,Boundary::uiWrites,Boundary::launches,Boundary::liveJobs,Boundary::maxLiveJobs,Boundary::reservations,sizeof(expected),Boundary::stackBytes);
}
