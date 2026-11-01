#define main catalogue_regression_main
#include "test_app_scan.cpp"
#undef main
#include <chrono>
using Clock=std::chrono::steady_clock;
int finishAsync(){
 const auto deadline=Clock::now()+std::chrono::seconds(3);
 while(Clock::now()<deadline){int r=Tab5AppCatalogue::scanStep();if(r)return r;std::this_thread::yield();}
 assert(false&&"scan failed to complete");return -9;
}
void retire(){
 Tab5AppCatalogue::endScan();MockVolume::release();MockTask::join();
 Tab5AppCatalogue::endScan();assert(!MockVolume::handles&&!MockVolume::leases);
}
void fixture(){
 MockTask::admit=false;MockTask::create=true;fresh();
 MockVolume::add("/Apps/old.elf");assert(Tab5AppCatalogue::beginScan(true)&&finish()==1);
 MockVolume::uiOpens=MockVolume::workerOpens=0;
 MockTask::admit=true;
}
int main(){using namespace Tab5AppCatalogue;using MockVolume::add;
 fixture();const uint32_t beforeEntry=revision();MockTask::armEntry();assert(beginScan(true));MockTask::waitEntry();
 endScan();MockTask::releaseEntry();MockTask::join();assert(scanStep()==-1);
 assert(!MockVolume::uiOpens&&!MockVolume::workerOpens&&revision()==beforeEntry);retire();
 puts("Pre-entry cancellation: no filesystem open on either thread");
 fixture();const uint32_t firstRevision=revision();const unsigned firstCount=count();
 add("/Apps/new.elf");MockVolume::arm("/Apps");assert(beginScan(true));MockVolume::waitBlocked();
 assert(!MockVolume::uiOpens&&workerRunning.load());
 for(unsigned i=0;i<1000;++i)assert(scanStep()==0&&revision()==firstRevision&&count()==firstCount);
 const auto cancellation=Clock::now();endScan();const auto cancellationUs=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-cancellation).count();
 assert(workerRunning.load()); // The I/O owner is still blocked; UI did not join it.
 const unsigned created=MockTask::creates;
 assert(beginScan(false)&&beginScan(true)&&beginScan(false)&&beginScan(true));
 assert(MockTask::creates==created&&scanStep()==0&&revision()==firstRevision);
 MockVolume::release();assert(finishAsync()==1);MockTask::join();
 assert(MockTask::creates==created+1&&find("/Apps/new.elf")>=0&&revision()==firstRevision+1);
 assert(!MockVolume::handles&&!MockVolume::leases&&!MockVolume::uiOpens);
 printf("Blocked I/O: 1000 UI polls without FS calls; cancel returned in %lld us; latest queued scan published after retirement\n",static_cast<long long>(cancellationUs));

 fixture();const uint32_t cancelledRevision=revision();MockVolume::arm("/Apps");assert(beginScan(true));MockVolume::waitBlocked();
 endScan();assert(workerRunning.load());MockVolume::release();MockTask::join();
 assert(scanStep()==-1&&revision()==cancelledRevision&&find("/Apps/old.elf")>=0);retire();

 fixture();add("/Apps/package",true);add("/Apps/package/main.elf");assert(beginScan(true)&&finishAsync()==1);MockTask::join();
 const uint32_t beforeError=revision();MockVolume::failPath="/Apps/package/main.elf";
 assert(beginScan(true)&&finishAsync()==-1);MockTask::join();
 assert(revision()==beforeError&&find("/Apps/package/main.elf")>=0);MockVolume::failPath.clear();retire();

 fixture();const uint32_t beforeWithdrawal=revision();MockVolume::arm("/Apps");assert(beginScan(true));MockVolume::waitBlocked();
 MockVolume::available=false;MockVolume::release();assert(finishAsync()==-1);MockTask::join();
 assert(revision()==beforeWithdrawal&&find("/Apps/old.elf")>=0);MockVolume::available=true;retire();

 fixture();MockTask::create=false;const unsigned previousFallbacks=MockTask::fallbacks;
 add("/Apps/fallback.elf");assert(beginScan(true)&&finish()==1);
 assert(find("/Apps/fallback.elf")>=0&&MockVolume::uiOpens&&MockTask::fallbacks==previousFallbacks+1&&!MockTask::active);retire();

 fixture();MockVolume::arm("/Apps");assert(beginScan(true));MockVolume::waitBlocked();
 assert(reset()&&addBuiltin("new-native","Native",2,1));
 MockVolume::release();MockTask::join();assert(scanStep()==-1);
 assert(count()==1&&!strcmp(at(0)->id,"new-native"));retire();

 fixture();const auto tableAllocations=MockHeap::allocations;
 for(unsigned i=0;i<100;++i){assert(beginScan(true)&&finishAsync()==1);MockTask::join();assert(!MockVolume::handles&&!MockVolume::leases);}
 assert(MockHeap::allocations==tableAllocations);retire();
 puts("PASS: actual asynchronous catalogue, cancellation/restart/reset, I/O failure/withdrawal, optional-task fallback and 100 clean task/file/lease cycles");
}
