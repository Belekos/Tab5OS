#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
class String {
 std::string value;
 public:String()=default;String(const char* s):value(s?s:""){}String(std::string s):value(std::move(s)){}
 size_t length()const{return value.size();}const char* c_str()const{return value.c_str();}
 void toLowerCase(){for(char& c:value)c=std::tolower(static_cast<unsigned char>(c));}
 bool operator==(const char* rhs)const{return value==rhs;}bool operator==(const String& rhs)const{return value==rhs.value;}
 bool operator!=(const char* rhs)const{return !(*this==rhs);}
};
using BaseType_t=int;using TickType_t=uint32_t;using TaskHandle_t=void*;
using SemaphoreHandle_t=std::timed_mutex*;
constexpr int pdTRUE=1,pdPASS=1;constexpr uint32_t portMAX_DELAY=0xffffffff;
uint32_t pdMS_TO_TICKS(uint32_t t){return t;}
int xSemaphoreTake(SemaphoreHandle_t m,TickType_t wait){if(wait==portMAX_DELAY){m->lock();return 1;}return m->try_lock_for(std::chrono::milliseconds(wait));}
void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
void vTaskDelete(void*){}
// A UI join would fail this test rather than actually sleeping 5.5 seconds.
void vTaskDelay(int){assert(!"unexpected blocking UI wait");}
uint32_t millis(){return 0;}
uint64_t esp_timer_get_time(){return 9000;}
template<class T>T constrain(T n,T a,T b){return std::min(b,std::max(n,a));}
bool isRemoteSource(const String& s){return std::string(s.c_str()).rfind("http",0)==0;}
bool isUnsupportedContainerPath(const String& s){return std::string(s.c_str()).find(".mp4")!=std::string::npos;}
bool isVideoPath(const String& s){auto t=std::string(s.c_str());return t.find(".avi")!=std::string::npos||t.find(".mjpeg")!=std::string::npos;}
String sourceName(const String& s){return s;}
namespace Tab5Resources{enum class Owner{Video};struct TaskBudget{explicit TaskBudget(unsigned){}};}
std::atomic<bool> memoryAllowed{true};
struct Admission{explicit operator bool()const{return memoryAllowed;}};
Admission tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::TaskBudget,const char*){return {};}
void tab5DiagnosticMark(int,const char*,const char*,int){}
void tab5TaskCheckpoint(const char*){}
struct Tab5StorageUse{explicit Tab5StorageUse(bool){}operator bool()const{return true;}};
// PRODUCTION_GUARD
#define private public
#include "Tab5Video.h"
#undef private
using Entry=void(*)(void*);
struct Job{Entry entry;void* argument;};
std::vector<Job> jobs;
bool taskCreationAllowed=true;
int tab5CreateAdmittedTask(Admission&,Entry entry,const char*,unsigned,void* argument,unsigned,TaskHandle_t* task,int){
 if(!taskCreationAllowed)return 0;
 *task=reinterpret_cast<void*>(jobs.size()+1);jobs.push_back({entry,argument});return pdPASS;
}
std::mutex workMutex;std::condition_variable workCv;bool entered=false,complete=false;
std::atomic<int> ownedResources{0},maxOwners{0};String startedSource;
bool holdPublication=false,publicationReached=false,allowPublication=false;
struct FakeLease{FakeLease(){int n=++ownedResources;maxOwners=std::max(maxOwners.load(),n);}~FakeLease(){--ownedResources;}};
void Tab5VideoEngine::logMemory(const char*)const{
 assert(ownedResources==0);
 std::unique_lock<std::mutex> lock(workMutex);
 if(holdPublication){publicationReached=true;workCv.notify_all();workCv.wait(lock,[]{return allowPublication;});}
}
void Tab5VideoEngine::releaseDecoderResources(){}
void Tab5VideoEngine::releasePlayback(){_clockStartedUs=_pauseStartedUs=0;}
void Tab5VideoEngine::taskMain(){
 FakeLease lease;
 {MutexGuard guard(_mutex);startedSource=_pendingSource;}
 {std::unique_lock<std::mutex> lock(workMutex);entered=true;workCv.notify_all();workCv.wait(lock,[]{return complete;});}
 struct Input{void close(){}} input;
 // PRODUCTION_CLEANUP
}
// PRODUCTION_METHODS
std::thread startJob(size_t index){
 {std::lock_guard<std::mutex> lock(workMutex);entered=complete=false;}
 auto job=jobs.at(index);std::thread thread([job]{job.entry(job.argument);});
 std::unique_lock<std::mutex> lock(workMutex);workCv.wait(lock,[]{return entered;});return thread;
}
void finishJob(std::thread& thread){
 {std::lock_guard<std::mutex> lock(workMutex);complete=true;}workCv.notify_all();thread.join();assert(ownedResources==0);
}
void seedFrame(Tab5VideoEngine& e){MutexGuard guard(e._mutex);e._frames[0]=static_cast<uint8_t*>(malloc(16));e._frames[0][0]=42;e._frameCapacity=16;e._frontFrame=0;e._frameGeneration=17;e._state.width=e._state.height=2;}
int main(){
 std::timed_mutex mutex;Tab5VideoEngine e;e._mutex=&mutex;e._sdAvailable=true;
 // Request replacement before the old worker has even copied startup fields.
 assert(e.playSource("one.avi","One"));assert(jobs.size()==1);seedFrame(e);
 Tab5VideoFrame old;assert(e.frame(old));assert(old.data[0]==42);
 assert(e.playSource("two.avi","Two"));assert(jobs.size()==1);assert(e._pendingSource=="one.avi");
 Tab5VideoSnapshot snapshot;assert(e.snapshot(snapshot));assert(snapshot.name=="Two");assert(snapshot.state==Tab5VideoState::Opening);assert(!e.frame(old));
 auto first=startJob(0);assert(startedSource=="one.avi");assert(e._stopRequested);
 assert(e.playSource("three.avi","Three"));e.service();assert(jobs.size()==1);
 // Equivalent to UI clearing its LVGL src, then notifying detachDisplay.
 e.detachDisplay();assert(e._frames[0]&&e._frames[0][0]==42);
 finishJob(first);assert(!e._taskRunning);assert(!e._frames[0]);assert(e._task==nullptr);
 e.service();assert(jobs.size()==2);assert(e._pendingSource=="three.avi");assert(!e._stopRequested);
 auto third=startJob(1);assert(startedSource=="three.avi");seedFrame(e);Tab5VideoFrame pinned;assert(e.frame(pinned));
 // Stop must cancel both active playback and a later queued replacement.
 assert(e.playSource("cancelled.avi"));e.stop();assert(!e._playQueued);assert(e._taskRunning);assert(pinned.data[0]==42);
 finishJob(third);assert(!e._taskRunning);assert(e._frames[0]);assert(pinned.data[0]==42); // still attached to LVGL
 assert(e.snapshot(snapshot));assert(snapshot.state==Tab5VideoState::Idle);e.service();assert(jobs.size()==2);
 e.detachDisplay();assert(!e._frames[0]);
 // Detach specifically after taskMain cleanup, before taskEntry publishes
 // completion. Neither side may miss the responsibility to free the frames.
 assert(e.playSource("publication-race.avi"));auto race=startJob(2);seedFrame(e);e.stop();
 {std::lock_guard<std::mutex> lock(workMutex);holdPublication=true;complete=true;}
 workCv.notify_all();
 {std::unique_lock<std::mutex> lock(workMutex);workCv.wait(lock,[]{return publicationReached;});}
 assert(e._taskRunning);e.detachDisplay();assert(e._frames[0]);
 {std::lock_guard<std::mutex> lock(workMutex);allowPublication=true;}workCv.notify_all();race.join();
 assert(!e._taskRunning);assert(!e._frames[0]);holdPublication=false;
 // SD availability can change while a replacement waits for a remote close.
 assert(e.playSource("https://fixture.invalid/stream.mjpeg"));auto remote=startJob(3);
 assert(e.playSource("missing.avi","Missing"));e.detachDisplay();e._sdAvailable=false;finishJob(remote);e.service();
 assert(jobs.size()==4);assert(e.snapshot(snapshot));assert(snapshot.state==Tab5VideoState::Error);assert(snapshot.name=="Missing");
 // Immediate allocation and task-creation errors retain truthful Error state.
 e._sdAvailable=true;memoryAllowed=false;assert(!e.playSource("oom.avi"));assert(!e._taskRunning);assert(e.snapshot(snapshot)&&snapshot.state==Tab5VideoState::Error);
 memoryAllowed=true;taskCreationAllowed=false;assert(!e.playSource("task-fail.avi"));assert(!e._taskRunning);assert(e.snapshot(snapshot)&&snapshot.state==Tab5VideoState::Error);
 assert(!e.playSource("invalid.mp4"));assert(maxOwners==1);
 std::cout<<"{\"passed\":true,\"jobsCreated\":"<<jobs.size()<<",\"maxConcurrentOwners\":"<<maxOwners<<",\"replacementBeforeWorkerEntry\":true,\"latestIntentWins\":true,\"stopCancelsPending\":true,\"detachedFramesWaitForWorker\":true,\"attachedFramesSurviveWorker\":true,\"detachDuringFinalPublication\":true,\"sdRevalidation\":true,\"allocationFailures\":true,\"noUiJoin\":true}"<<std::endl;
}
