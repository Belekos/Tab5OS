#include <atomic>
#include <cstdint>
#include <cassert>
#include <functional>
#include <iostream>
#include "Tab5AudioPriority.h"
#include "Tab5AudioWorkerLifecycle.h"
unsigned checks=0;uint32_t nowMs=0;bool busy=false,mediaPending=false;void* currentTask=nullptr;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";std::abort();}}while(0)
uint32_t millis(){return nowMs;}bool tab5NetworkBusy(){return busy;}bool tab5NetworkMediaPending(){return mediaPending;}
void* xTaskGetCurrentTaskHandle(){return currentTask;}
unsigned uxQueueMessagesWaiting(unsigned*p){return *p;}constexpr int WL_CONNECTED=3;
struct Wifi {int state=WL_CONNECTED,signal=-63;unsigned queries=0,sleepCalls=0;bool sleeping=false,sleepOk=true;std::function<void()> onRssi;int status(){return state;}int RSSI(){++queries;if(onRssi)onRssi();return signal;}bool setSleep(bool value){++sleepCalls;if(sleepOk)sleeping=value;return sleepOk;}} WiFi;
struct Device{unsigned calls=0;void service(){++calls;}}Tab5AudioDevice;
enum class Tab5AudioMode {Idle,Local,Radio,Recording};
struct Tab5AudioEngine {
 Tab5AudioWorkerLifecycle _workerLifecycle;Tab5AudioPriorityControl _memoryPriority;
 bool _wifiEnabled=true,_wifiPending=false,_wifiScanRunning=false,_wifiSleeping=false,_wifiSignalSampled=false,_workerRetryPending=false,_priorityResume=false,_priorityCommandPending=false;
 std::atomic<bool> _wifiPowerSave{false},_wifiSignalRequested{false},_idleReleaseRequested{false};
 uint32_t _lastHousekeepingAttempt=0,_lastWorkerAttempt=0,_lastWifiSignalAt=0,_networkLastBusy=0,_wifiPowerAttemptAt=0;
 int32_t _wifiRssiCached=0;Tab5AudioMode _mode=Tab5AudioMode::Idle;
 void* _audioTask=nullptr;unsigned queued=0;unsigned* _commandQueue=&queued;
 void *_localSource=nullptr,*_streamSource=nullptr,*_radioSource=nullptr,*_radioLease=nullptr,*_mp3=nullptr,*_wav=nullptr,*_aac=nullptr,*_flac=nullptr,*_recordFile=nullptr;
 unsigned starts=0,snapshots=0,statuses=0;bool failStart=false,lastEssential=false;
 struct StateGuard{StateGuard(const Tab5AudioEngine&,int){}operator bool() const{return true;}};
 void service();void serviceIdleMemoryPriority();void serviceWifiSignal();void serviceWifiPower();bool wifiPowerTarget(uint32_t);
 // SIGNAL_REQUEST
 bool startAudioTask(bool essential){++starts;lastEssential=essential;CHECK(_workerLifecycle.tryStart());if(failStart){_workerLifecycle.failedStart();return false;}_audioTask=this;_workerLifecycle.started();return true;}
 void publishSnapshot(){++snapshots;}void setStatus(const char*){++statuses;}
 void finish(){if(!_workerLifecycle.active())return;currentTask=this;serviceWifiPower();serviceWifiSignal();queued=0;_audioTask=nullptr;currentTask=nullptr;_workerLifecycle.stopped();}
 void tick(uint32_t t){nowMs=t;service();finish();}
};
// METHODS
void reset(){WiFi=Wifi{};nowMs=0;busy=mediaPending=false;currentTask=nullptr;}
int main(){
 unsigned stableStarts[2],stableQueries[2];
 for(bool powerSave:{false,true}){reset();Tab5AudioEngine s;s._wifiPowerSave=powerSave;for(unsigned t=0;t<=600000;t+=100)s.tick(t);stableStarts[powerSave]=s.starts;stableQueries[powerSave]=WiFi.queries;CHECK(s.starts==(PATCHED?1u:120u));CHECK(WiFi.queries==(PATCHED?1u:120u));CHECK(WiFi.sleepCalls==(powerSave?1u:0u));}
 reset();Tab5AudioEngine s;s.tick(5000);auto starts=s.starts,queries=WiFi.queries;
 s.requestWifiSignal();s.tick(6000);CHECK(s.starts==starts);s.tick(10000);CHECK(s.starts==starts+1&&WiFi.queries==queries+1);
 s._wifiEnabled=false;s.requestWifiSignal();s.tick(20000);CHECK(s.starts==starts+1);
 s._wifiEnabled=true;WiFi.state=0;s.tick(30000);CHECK(WiFi.queries==queries+1);
#if PATCHED
 CHECK(s.starts==starts+1&&s._wifiRssiCached==0&&!s._wifiSignalSampled);
#endif
 WiFi.state=WL_CONNECTED;s.tick(40000);CHECK(WiFi.queries==queries+2);
 auto count=s.starts;s._mode=Tab5AudioMode::Radio;s.requestWifiSignal();s.tick(50000);CHECK(WiFi.queries==queries+2);
#if PATCHED
 CHECK(s.starts==count);
#endif
 s._mode=Tab5AudioMode::Idle;s.tick(60000);CHECK(WiFi.queries==queries+3);
 s._wifiPending=true;s.requestWifiSignal();queries=WiFi.queries;count=s.starts;s.tick(70000);CHECK(WiFi.queries==queries);
#if PATCHED
 CHECK(s.starts==count);
#endif
 s._wifiPending=false;s._wifiScanRunning=true;s.tick(80000);CHECK(WiFi.queries==queries);s._wifiScanRunning=false;s.tick(90000);CHECK(WiFi.queries==queries+1);
 // Acquisition is acknowledged without a decoder stack; held priorities suppress optional work.
 auto token=s._memoryPriority.request();CHECK(token);s.requestWifiSignal();count=s.starts;s.tick(100000);CHECK(s._memoryPriority.state()==Tab5AudioPriorityControl::Held&&s.starts==count);s._memoryPriority.release(token);s.tick(110000);CHECK(s._memoryPriority.state()==Tab5AudioPriorityControl::Idle&&s.starts==count+1);
 // Reclaimer backoff is optional only: user commands remain immediate.
 s._idleReleaseRequested=true;s.requestWifiSignal();count=s.starts;s.tick(120000);CHECK(s.starts==count);s.queued=1;s.tick(120001);CHECK(s.starts==count+1&&s.lastEssential);
 s.failStart=true;s.queued=1;count=s.starts;s.tick(121000);CHECK(s.starts==count+1&&s._workerRetryPending&&s.statuses==1);s.tick(121499);CHECK(s.starts==count+1);s.tick(121500);CHECK(s.starts==count+2&&s.statuses==2);s.failStart=false;s.tick(122000);CHECK(!s._workerRetryPending&&s.queued==0);
 // Lifecycle acquire stops UI from touching worker-owned ordinary state.
 CHECK(s._workerLifecycle.tryStart());s._workerLifecycle.started();s._audioTask=&s;count=s.starts;queries=WiFi.queries;nowMs=130000;s.service();CHECK(s.starts==count&&WiFi.queries==queries);s.finish();
 // A new signal request arriving during RPC remains pending.
 s.requestWifiSignal();WiFi.onRssi=[&]{s.requestWifiSignal();};s.tick(140000);CHECK(s._wifiSignalRequested);WiFi.onRssi={};queries=WiFi.queries;s.tick(144999);CHECK(WiFi.queries==queries);s.tick(145000);CHECK(WiFi.queries==queries+1);
 // Sleep is changed only when eligible; wake transition also reacts to shared-network activity.
 reset();Tab5AudioEngine p;p._wifiPowerSave=true;p.tick(5000);CHECK(p._wifiSleeping);busy=true;p.tick(10000);CHECK(!p._wifiSleeping);busy=false;p.tick(14999);CHECK(!p._wifiSleeping);p.tick(15000);CHECK(p._wifiSleeping);mediaPending=true;p.tick(20000);CHECK(!p._wifiSleeping);mediaPending=false;
 WiFi.sleepOk=false;p.tick(25000);CHECK(!p._wifiSleeping);unsigned calls=WiFi.sleepCalls;p.tick(29999);CHECK(WiFi.sleepCalls==calls);WiFi.sleepOk=true;p.tick(30000);CHECK(p._wifiSleeping);
 // Unsigned elapsed arithmetic over millis rollover.
 reset();Tab5AudioEngine w;w._lastHousekeepingAttempt=UINT32_MAX-3000;w._lastWifiSignalAt=UINT32_MAX-3000;w._wifiSignalSampled=true;w.requestWifiSignal();w.tick(1998);CHECK(w.starts==0);w.tick(1999);CHECK(w.starts==1&&WiFi.queries==1);
 std::cout<<"{\"checks\":"<<checks<<",\"simulationMilliseconds\":600000,\"stableWorkersPowerSaveOff\":"<<stableStarts[0]<<",\"stableWorkersPowerSaveOn\":"<<stableStarts[1]<<",\"rssiRequestsPowerSaveOff\":"<<stableQueries[0]<<",\"rssiRequestsPowerSaveOn\":"<<stableQueries[1]<<",\"physicalTimingMeasured\":false}\n";
}
