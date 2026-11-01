#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <iostream>
using TaskHandle_t=void*;using BaseType_t=int;using TickType_t=unsigned;using TaskFunction_t=void(*)(void*);
constexpr int pdPASS=1,pdFAIL=0;constexpr unsigned DECODER_TASK_PRIORITY=5;
#define pdMS_TO_TICKS(x) (x)
static std::atomic<unsigned> checks{0},starts{0},tasks{0},stackBytes{0},reserved{0},denied{0};
static std::atomic<bool> allowAdmission{true},allowCreation{true};
// TEST_EVENT_WAITS
static std::atomic<unsigned> pausedTickPolls{0},queueWaits{0};
static std::atomic<bool> recordPausedWaits{false};
static std::atomic<unsigned> timeOffset{0};
static thread_local TaskHandle_t currentTask=nullptr;
static std::mutex threadMutex;static std::vector<std::thread> threads;
static const auto epoch=std::chrono::steady_clock::now();
uint32_t millis(){return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-epoch).count())+timeOffset.load();}
void vTaskDelay(unsigned ms){if(currentTask&&ms==1&&recordPausedWaits)++pausedTickPolls;std::this_thread::sleep_for(std::chrono::milliseconds(ms?ms:1));}
TaskHandle_t xTaskGetCurrentTaskHandle(){return currentTask;}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t){return 8192;}
void vTaskDelete(TaskHandle_t){}
void tab5TaskStarted(const char*){}

struct Serial{template<class... A>void printf(const char*,A...){} } Tab5DebugSerial;
[[noreturn]]static void fail(const char* expression,int line){std::cerr<<"CHECK failed "<<line<<": "<<expression<<'\n';std::abort();}
#define CHECK(x) do{++checks;if(!(x))fail(#x,__LINE__);}while(0)
namespace Tab5Resources {
enum class Owner{Audio,Tools};enum class Denial{None,Internal};struct AdmissionReport{Denial reason=Denial::None;};
struct Budget{size_t internal=0;};constexpr Budget TaskBudget(size_t stack){return {stack+4096};}
class Reservation{bool held=false;public:Reservation()=default;explicit Reservation(bool h):held(h){if(h)++reserved;}~Reservation(){release();}Reservation(Reservation&& r):held(r.held){r.held=false;}Reservation&operator=(Reservation&&r){release();held=r.held;r.held=false;return *this;}explicit operator bool()const{return held;}void release(){if(held){held=false;--reserved;}}};
struct Manager{Reservation reserve(Owner,const Budget&){if(!allowAdmission){++denied;return{};}return Reservation(true);}};
}
static Tab5Resources::Manager Tab5MemoryManager;
Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner owner,const Tab5Resources::Budget&budget,const char*,Tab5Resources::AdmissionReport* report=nullptr){if(report)report->reason=allowAdmission?Tab5Resources::Denial::None:Tab5Resources::Denial::Internal;return Tab5MemoryManager.reserve(owner,budget);}
int xTaskCreatePinnedToCore(TaskFunction_t f,const char*,uint32_t stack,void* arg,unsigned,TaskHandle_t*handle,int){
 if(!allowCreation)return pdFAIL;
 auto id=reinterpret_cast<void*>(uintptr_t(starts.fetch_add(1)+1));*handle=id;++tasks;stackBytes+=stack;
 std::lock_guard<std::mutex> guard(threadMutex);
 threads.emplace_back([=](){currentTask=id;f(arg);vTaskDelay(2);stackBytes-=stack;--tasks;});
 vTaskDelay(1); // Exercise scheduling before the creation function returns.
 return pdPASS;
}
#include "Tab5Task.h"
#include "Tab5AudioPriority.h"
#include "Tab5AudioWorkerLifecycle.h"
struct Tab5AudioCommand{int value;};
struct Queue{std::mutex mutex;std::condition_variable changed;std::deque<int> q;};
using QueueHandle_t=Queue*;
unsigned uxQueueMessagesWaiting(QueueHandle_t q){std::lock_guard<std::mutex> g(q->mutex);return q->q.size();}
void post(Queue&q,int value){std::lock_guard<std::mutex> g(q.mutex);q.q.push_back(value);q.changed.notify_one();}
int xQueuePeek(QueueHandle_t q,Tab5AudioCommand*out,unsigned timeout){++queueWaits;std::unique_lock<std::mutex>g(q->mutex);if(!q->changed.wait_for(g,std::chrono::milliseconds(timeout),[&]{return !q->q.empty();}))return 0;out->value=q->q.front();return 1;}
struct Tab5StorageUse{explicit Tab5StorageUse(bool){}explicit operator bool()const{return true;}};
struct Device{std::atomic<unsigned> services{0};void service(){++services;}}Tab5AudioDevice;
enum class Tab5AudioMode{Idle,Local,Radio,Recording};
class Tab5AudioEngine{
public:
 class StateGuard{std::unique_lock<std::recursive_mutex> lock;public:StateGuard(const Tab5AudioEngine&e,unsigned=0):lock(e.state,std::try_to_lock){}explicit operator bool()const{return lock.owns_lock();}};
 mutable std::recursive_mutex state;
 std::atomic<TaskHandle_t> _audioTask{nullptr};Tab5AudioWorkerLifecycle _workerLifecycle;
 std::atomic<bool> _idleReleaseRequested{false};uint32_t _lastHousekeepingAttempt=0,_lastWorkerAttempt=0;bool _workerRetryPending=false;
 std::atomic<bool> _paused{false};
 bool _wifiEnabled=false,_wifiPending=false,_wifiScanRunning=false,_wifiScanQueued=false,_playRadioAfterWifi=false,_radioReconnectPending=false;
 bool _priorityResume=false,_priorityCommandPending=false,_recordFile=false;
 void *_localSource=nullptr,*_streamSource=nullptr,*_radioSource=nullptr,*_radioLease=nullptr,*_mp3=nullptr,*_wav=nullptr,*_aac=nullptr,*_flac=nullptr;
 Tab5AudioMode _mode=Tab5AudioMode::Idle;void*_stateMutex=reinterpret_cast<void*>(1);Queue queue;QueueHandle_t _commandQueue=&queue;int _audioCore=0;
 Tab5AudioPriorityControl _memoryPriority;
 std::atomic<unsigned> heavy{0},snapshots{0},handled{0},loopOwners{0},maxLoopOwners{0},signals{0};std::atomic<bool> insideHeavy{false};unsigned pendingLoops=0;
 static void audioTaskEntry(void*);bool canRetireAudioTask()const;size_t reclaimIdleWorker();bool startAudioTask(bool);void service();void serviceIdleMemoryPriority();uint32_t acquireMemoryPriority(uint32_t);
 bool decoderRunning()const{return _mp3||_wav||_aac||_flac;}
 void assertWorker(){CHECK(currentTask!=nullptr);CHECK(currentTask==_audioTask.load());CHECK(!_workerLifecycle.starting());}
 void serviceMemoryPriority(){assertWorker();auto t=_memoryPriority.begin();if(t)_memoryPriority.acknowledge(t,!_recordFile);t=_memoryPriority.released();if(t)_memoryPriority.complete(t);}
 void drainCommands(){assertWorker();for(int n=0;n<4;++n){int cmd=0;{std::lock_guard<std::mutex>g(queue.mutex);if(queue.q.empty())return;cmd=queue.q.front();queue.q.pop_front();}++handled;
 switch(cmd){case 1:_mode=Tab5AudioMode::Local;_mp3=reinterpret_cast<void*>(1);break;case 2:_mode=Tab5AudioMode::Idle;_mp3=nullptr;_recordFile=false;break;case 3:_wifiEnabled=true;_wifiPending=true;pendingLoops=4;break;case 4:_wifiEnabled=true;_wifiScanRunning=true;pendingLoops=4;break;case 5:_mode=Tab5AudioMode::Recording;_recordFile=true;break;case 6:_mode=Tab5AudioMode::Radio;_mp3=nullptr;break;case 7:_paused=true;break;case 8:_paused=false;break;}
 }}
 void serviceDecoder(){assertWorker();CHECK(!insideHeavy.exchange(true));++heavy;if(pendingLoops&&!--pendingLoops){_wifiPending=false;_wifiScanRunning=false;}insideHeavy=false;}
 void serviceWifiSignal(){assertWorker();++signals;}
 void publishSnapshot(){++snapshots;}
 void setStatus(const char*){}
};
// PRODUCTION_METHODS
template<class Predicate> static bool waitFor(const Predicate&predicate,unsigned limit=4000){auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(limit);while(!predicate()){if(std::chrono::steady_clock::now()>until)return false;vTaskDelay(1);}return true;}
static void drain(Tab5AudioEngine&e){CHECK(waitFor([&]{e.service();return !e._workerLifecycle.active()&&uxQueueMessagesWaiting(&e.queue)==0;}));CHECK(waitFor([]{return tasks.load()==0;}));}
static void stop(Tab5AudioEngine&e){post(e.queue,2);drain(e);}
int main(){
 Tab5AudioEngine e;
 // No command means no 16KiB permanent task and no decoder/RPC fallback on UI.
 for(unsigned i=0;i<200;++i)e.service();CHECK(starts==0);CHECK(e.heavy==0);CHECK(stackBytes==0);
 // Failed allocation and task creation retain queued intent, then retry safely.
 allowAdmission=false;post(e.queue,1);e.service();CHECK(!e._workerLifecycle.active());CHECK(uxQueueMessagesWaiting(&e.queue)==1);CHECK(e.heavy==0);CHECK(reserved==0);
 allowAdmission=true;allowCreation=false;timeOffset+=501;e.service();CHECK(!e._workerLifecycle.active());CHECK(reserved==0);CHECK(uxQueueMessagesWaiting(&e.queue)==1);
 allowCreation=true;timeOffset+=501;e.service();CHECK(waitFor([&]{return e.handled.load()==1;}));CHECK(e._workerLifecycle.active());CHECK(stackBytes==16384);CHECK(reserved==0);
 e.reclaimIdleWorker();for(unsigned i=0;i<30;++i)e.service();CHECK(e._workerLifecycle.active());stop(e);CHECK(stackBytes==0);
 // Recording and WiFi progress are pinned; a cache callback never aborts them.
 post(e.queue,5);e.service();CHECK(waitFor([&]{return e.handled.load()==3;}));e.reclaimIdleWorker();vTaskDelay(30);CHECK(e._workerLifecycle.active());stop(e);
 for(int cmd:{3,4}){auto old=e.handled.load();post(e.queue,cmd);e.service();CHECK(waitFor([&]{return e.handled.load()>old;}));CHECK(e._workerLifecycle.active());drain(e);CHECK(stackBytes==0);}
 // Paused radio without resources can retire; the next command restarts it.
 post(e.queue,6);drain(e);CHECK(!e._workerLifecycle.active());
 // Optional WiFi housekeeping yields to low SRAM and is deferred after reclaim.
 auto old=starts.load();e.reclaimIdleWorker();e.service();CHECK(starts==old);timeOffset+=5001;allowAdmission=false;e.service();CHECK(starts==old);CHECK(e.heavy>0);allowAdmission=true;timeOffset+=5001;drain(e);CHECK(starts==old+1);
 // Silent video leases from fully idle audio need no new internal stack.
 e._mode=Tab5AudioMode::Idle;allowAdmission=false;auto beforeStarts=starts.load();
 std::atomic<uint32_t>token{0};std::thread lease([&]{currentTask=reinterpret_cast<void*>(999999);token=e.acquireMemoryPriority(1500);});
 CHECK(waitFor([&]{e.service();return token.load()!=0;}));lease.join();
 CHECK(!e._workerLifecycle.active());CHECK(starts==beforeStarts);CHECK(stackBytes==0);
 auto held=token.load();CHECK(e._memoryPriority.state(held)==Tab5AudioPriorityControl::Held);
 for(unsigned i=0;i<100;++i){timeOffset+=100;e.service();}CHECK(starts==beforeStarts);
 // Queue intent is retained under low memory, then begins after release/admission.
 post(e.queue,1);e.service();CHECK(uxQueueMessagesWaiting(&e.queue)==1);
 CHECK(!e._workerLifecycle.active());e._memoryPriority.release(held);e.service();
 CHECK(e._memoryPriority.state()==Tab5AudioPriorityControl::Idle);
 allowAdmission=true;timeOffset+=501;e.service();CHECK(waitFor([&]{return uxQueueMessagesWaiting(&e.queue)==0;}));stop(e);
 // Non-idle state still takes the owning worker path rather than UI resource work.
 e._mode=Tab5AudioMode::Radio;token=0;
 std::thread lease2([&]{currentTask=reinterpret_cast<void*>(999999);token=e.acquireMemoryPriority(1500);});
 CHECK(waitFor([&]{e.service();return token.load()!=0;}));lease2.join();CHECK(e._workerLifecycle.active());e._memoryPriority.release(token);drain(e);
 // Expired requests and stale releases cannot affect a later generation.
 e._mode=Tab5AudioMode::Idle;auto expired=e._memoryPriority.request();e._memoryPriority.release(expired);e.service();CHECK(e._memoryPriority.state()==Tab5AudioPriorityControl::Idle);
 auto next=e._memoryPriority.request();e.service();CHECK(e._memoryPriority.state(next)==Tab5AudioPriorityControl::Held);
 e._memoryPriority.release(expired);CHECK(e._memoryPriority.state(next)==Tab5AudioPriorityControl::Held);e._memoryPriority.release(next);e.service();
 // Commands at either side of retirement are not lost. Exercise reuse on both
 // cores with allocation publication, joining and stack accounting repeatedly.
 e._wifiEnabled=false;
 unsigned rounds=400;
 for(unsigned i=0;i<rounds;++i){auto count=e.handled.load();post(e.queue,6);e.service();std::thread producer([&]{post(e.queue,6);});producer.join();drain(e);CHECK(e.handled==count+2);CHECK(stackBytes==0);CHECK(reserved==0);}
 // Paused local playback preserves its decoder but waits on the command
 // queue instead of waking on every scheduler tick. Stop still wakes it.
 post(e.queue,1);e.service();CHECK(waitFor([&]{return e._mp3!=nullptr;}));
 post(e.queue,7);CHECK(waitFor([&]{return e._paused.load();}));
 pausedTickPolls=0;queueWaits=0;recordPausedWaits=true;vTaskDelay(45);recordPausedWaits=false;
 CHECK(TEST_EVENT_WAITS ? pausedTickPolls==0 : pausedTickPolls>0);
 CHECK(TEST_EVENT_WAITS ? queueWaits>0 : queueWaits==0);
 stop(e);CHECK(stackBytes==0);
 {std::lock_guard<std::mutex>g(threadMutex);for(auto&t:threads)t.join();}
 CHECK(tasks==0);CHECK(stackBytes==0);CHECK(reserved==0);
 std::cout<<"{\"checks\":"<<checks.load()<<" ,\"pausedOneTickSleeps\":"<<pausedTickPolls.load()<<" ,\"queueWaits\":"<<queueWaits.load()<<",\"repeatedCycles\":"<<rounds<<",\"workersCreated\":"<<starts.load()<<",\"admissionFailuresExercised\":"<<denied.load()<<",\"stackBytesAfter\":"<<stackBytes.load()<<",\"pendingReservationsAfter\":"<<reserved.load()<<"}\n";
}
