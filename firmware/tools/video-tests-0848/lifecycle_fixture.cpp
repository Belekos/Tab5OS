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
void beforeLock();
int xSemaphoreTake(SemaphoreHandle_t m,TickType_t wait){beforeLock();if(wait==portMAX_DELAY){m->lock();return 1;}return m->try_lock_for(std::chrono::milliseconds(wait));}
void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
std::atomic<unsigned> deletedTasks{0};
void vTaskDelete(void*){++deletedTasks;}
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
std::atomic<bool> storageAllowed{true};
struct Tab5StorageUse{explicit Tab5StorageUse(bool){}operator bool()const{return storageAllowed;}};
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

std::mutex workMutex;std::condition_variable workCv;
unsigned entries=0;std::vector<bool> releaseRun(2048,false);
std::vector<std::string> sources;
std::atomic<int> ownedResources{0},maxOwners{0};
bool interceptStop=false,stopBlocked=false,allowStop=false;
std::thread::id stopThread;
void beforeLock(){
 std::unique_lock<std::mutex> lock(workMutex);
 if(interceptStop && std::this_thread::get_id()==stopThread){
  stopBlocked=true;workCv.notify_all();workCv.wait(lock,[]{return allowStop;});
 }
}
struct FakeLease{FakeLease(){int n=++ownedResources;maxOwners=std::max(maxOwners.load(),n);}~FakeLease(){--ownedResources;}};
void Tab5VideoEngine::logMemory(const char*)const{assert(ownedResources==0);}
void Tab5VideoEngine::releaseDecoderResources(){}
void Tab5VideoEngine::releasePlayback(){_clockStartedUs=_pauseStartedUs=0;}
void Tab5VideoEngine::taskMain(){
 FakeLease lease;String source;
 {MutexGuard guard(_mutex);source=_pendingSource;}
 {
  std::unique_lock<std::mutex> lock(workMutex);sources.emplace_back(source.c_str());
  unsigned mine=++entries;workCv.notify_all();
  workCv.wait(lock,[mine]{return releaseRun[mine];});
 }
 struct Input{void close(){}} input;
 // PRODUCTION_CLEANUP
}
// PRODUCTION_METHODS
void waitForRun(unsigned n){std::unique_lock<std::mutex> lock(workMutex);assert(workCv.wait_for(lock,std::chrono::seconds(3),[n]{return entries>=n;}));}
void finishRun(unsigned n){{std::lock_guard<std::mutex> lock(workMutex);releaseRun[n]=true;}workCv.notify_all();}
std::thread startJob(size_t index){auto job=jobs.at(index);return std::thread([job]{job.entry(job.argument);});}
void seedFrame(Tab5VideoEngine& e){MutexGuard guard(e._mutex);assert(!e._frames[0]);e._frames[0]=static_cast<uint8_t*>(malloc(16));e._frames[0][0]=42;e._frameCapacity=16;e._frontFrame=0;e._frameGeneration=17;e._state.width=e._state.height=2;}
int main(){
 std::timed_mutex mutex;Tab5VideoEngine e;e._mutex=&mutex;e._sdAvailable=true;
 unsigned checks=0;
 auto check=[&](bool value){++checks;assert(value);};
 // Already queued before the owner even enters; only latest startup intent wins.
 check(e.playSource("one.avi","One"));check(jobs.size()==1);seedFrame(e);
 Tab5VideoFrame old;check(e.frame(old));check(old.data[0]==42);
 check(e.playSource("two.avi","Two"));check(e._pendingSource=="one.avi");
 Tab5VideoSnapshot snapshot;check(e.snapshot(snapshot));check(snapshot.name=="Two");check(!e.frame(old));
 auto thread=startJob(0);waitForRun(1);check(sources[0]=="one.avi");
 check(e.playSource("three.avi","Three"));e.service();check(jobs.size()==1);
 e.detachDisplay();check(e._frames[0]!=nullptr);finishRun(1);waitForRun(2);
 check(sources[1]=="three.avi");check(e._taskRunning);check(jobs.size()==1);check(deletedTasks==0);check(!e._frames[0]);
 // Retain a frame still referenced by LVGL; next preparation must not unpin it.
 seedFrame(e);check(e.frame(old));const uint8_t pinned=e._uiFrame;
 check(e.playSource("four.avi"));finishRun(2);waitForRun(3);
 check(e._uiFrame==pinned);check(old.data[0]==42);check(jobs.size()==1);
 // Stop cancels both current and queued intents, no timer-driven respawn.
 check(e.playSource("cancelled.avi"));e.stop();check(!e._playQueued);finishRun(3);thread.join();
 check(!e._taskRunning);check(deletedTasks==1);e.service();check(jobs.size()==1);check(old.data[0]==42);
 e.detachDisplay();check(!e._frames[0]);
 // Stop atomically begins before the worker clears the old abort flag, then
 // acquires the mutex afterward: the second cancellation store is essential.
 check(e.playSource("race-one.avi"));thread=startJob(1);waitForRun(4);
 check(e.playSource("race-two.avi"));
 {
  std::lock_guard<std::mutex> lock(workMutex);interceptStop=true;
 }
 std::thread stop([&]{{std::lock_guard<std::mutex> lock(workMutex);stopThread=std::this_thread::get_id();}e.stop();});
 {std::unique_lock<std::mutex> lock(workMutex);check(workCv.wait_for(lock,std::chrono::seconds(3),[]{return stopBlocked;}));}
 finishRun(4);waitForRun(5);check(!e._stopRequested);
 {std::lock_guard<std::mutex> lock(workMutex);allowStop=true;}workCv.notify_all();stop.join();check(e._stopRequested);
 finishRun(5);thread.join();interceptStop=false;e.detachDisplay();
 // Storage disappears while a local replacement waits for a remote close.
 check(e.playSource("https://fixture.invalid/live.mjpeg"));thread=startJob(2);waitForRun(6);
 check(e.playSource("removed.avi","Removed"));
 {MutexGuard guard(e._mutex);e._sdAvailable=false;}
 e.detachDisplay();finishRun(6);thread.join();e.service();
 check(jobs.size()==3);check(e.snapshot(snapshot));check(snapshot.state==Tab5VideoState::Error);check(snapshot.name=="Removed");
 // Simulate the narrow request-after-retirement window; UI service handles it.
 {MutexGuard guard(e._mutex);e._queuedPlay.source="late.avi";e._queuedPlay.name="Late";e._queuedPlay.format="avi";e._playQueued=true;e._sdAvailable=true;}
 e.service();check(jobs.size()==4);thread=startJob(3);waitForRun(7);e.stop();finishRun(7);thread.join();e.detachDisplay();
 // Rapid sequential switches reuse one task; every owner and retained frame closes.
 check(e.playSource("cycle.avi"));thread=startJob(4);unsigned run=8;waitForRun(run);
 for(unsigned i=0;i<200;++i){check(e.playSource("cycle.avi"));e.detachDisplay();finishRun(run++);waitForRun(run);check(jobs.size()==5);check(ownedResources==1);}
 e.stop();e.detachDisplay();finishRun(run);thread.join();check(!e._taskRunning);check(ownedResources==0);
 memoryAllowed=false;check(!e.playSource("oom.avi"));check(!e._taskRunning);check(e.snapshot(snapshot)&&snapshot.state==Tab5VideoState::Error);
 memoryAllowed=true;taskCreationAllowed=false;check(!e.playSource("task-fail.avi"));check(!e._taskRunning);check(!e.playSource("invalid.mp4"));
 check(maxOwners==1);check(deletedTasks==5);
 std::cout<<"{\"passed\":true,\"assertions\":"<<checks<<",\"jobsCreated\":"<<jobs.size()<<",\"playbacks\":"<<entries<<",\"maxConcurrentOwners\":"<<maxOwners<<",\"stopPrepareRace\":true,\"pinnedFramePreserved\":true,\"sdRevalidation\":true,\"noUiJoin\":true}"<<std::endl;
}
