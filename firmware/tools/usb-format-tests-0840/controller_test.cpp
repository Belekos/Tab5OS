#include "controller.cpp"
#include <iostream>
static unsigned checks=0;
#define CHECK(x) do{++checks;assert(x);}while(0)
static void connect(){msc_host_event_t e{};e.event=msc_host_event_t::MSC_DEVICE_CONNECTED;e.device.address=1;eventCallback(&e,nullptr);Tab5UsbStorage::serviceWorker();}
static void disconnect(){msc_host_event_t e{};e.event=msc_host_event_t::MSC_DEVICE_DISCONNECTED;e.device.handle=handle;eventCallback(&e,nullptr);Tab5UsbStorage::serviceWorker();}
static void run(){auto fn=queuedTask;queuedTask=nullptr;CHECK(fn);fn(nullptr);}
static void clear(){
 using namespace Tab5UsbStorage;
 if(events.load())vQueueDelete(events);events=nullptr;device=nullptr;vfs=nullptr;present=false;ready=false;accept=false;autoMount=true;namespaceUsed=false;refs=0;request=0;gen=1;rev=0;connection=1;formatConnection=0;trackedDevice=nullptr;formatting=FormatState::Idle;formatWorkerActive=false;capacitySectors=0;capacitySectorSize=0;address=0;
 queuedTask=nullptr;failTask=failMount=sdConflict=failInstall=failFormat=false;exclusiveAvailable=true;rawCalls=mountCalls=unmountCalls=uninstallCalls=delayCalls=0;syncError=ESP_OK;sectors=131072;sectorSize=512;onDelay={};onRaw={};onCheckpoint={};CHECK(begin());
}
int main(){using namespace Tab5UsbStorage;
 clear();failMount=true;connect();CHECK(attached());CHECK(!mounted());CHECK(capacityBytes()==64ULL*1024*1024);CHECK(rawCalls==0);CHECK(supported(Format::Fat32));CHECK(requestFormat(Format::Fat32,connectionToken()));CHECK(pending());CHECK(formatState()==FormatState::Waiting);serviceWorker();CHECK(queuedTask);CHECK(rawCalls==0);run();CHECK(rawCalls==1);CHECK(lastFat32);CHECK(mounted());CHECK(formatState()==FormatState::Succeeded);CHECK(!pending());
 clear();connect();const uint32_t old=connectionToken();disconnect();connect();CHECK(connectionToken()!=old);CHECK(!requestFormat(Format::Fat32,old));CHECK(rawCalls==0);
 clear();connect();CHECK(requestFormat(Format::Fat32,connectionToken()));CHECK(cancelFormat());serviceWorker();CHECK(!queuedTask);CHECK(rawCalls==0);CHECK(mounted());CHECK(formatState()==FormatState::Cancelled);
 clear();connect();CHECK(requestFormat(Format::Fat32,connectionToken()));serviceWorker();CHECK(cancelFormat());run();CHECK(rawCalls==0);CHECK(!pending());
 clear();connect();uint32_t lease;CHECK(retain(lease));CHECK(requestFormat(Format::Fat32,connectionToken()));uint32_t rejected;CHECK(!retain(rejected));serviceWorker();onDelay=[](){CHECK(rawCalls==0);release();};run();CHECK(delayCalls==1);CHECK(rawCalls==1);CHECK(!valid(lease));
 clear();connect();exclusiveAvailable=false;CHECK(requestFormat(Format::Fat32,connectionToken()));serviceWorker();onDelay=[](){CHECK(rawCalls==0);CHECK(cancelFormat());};run();CHECK(rawCalls==0);CHECK(mounted());
 clear();connect();CHECK(requestFormat(Format::Fat32,connectionToken()));serviceWorker();onRaw=[](){CHECK(!cancelFormat());disconnect();CHECK(uninstallCalls==0);};run();CHECK(formatState()==FormatState::Failed);CHECK(!mounted());serviceWorker();CHECK(uninstallCalls==1);CHECK(!pending());CHECK(capacityBytes()==0);
 clear();connect();CHECK(requestFormat(Format::Fat32,connectionToken()));serviceWorker();disconnect();CHECK(uninstallCalls==0);run();CHECK(rawCalls==0);serviceWorker();CHECK(uninstallCalls==1);
 clear();automatic(false);connect();CHECK(attached());CHECK(!mounted());CHECK(mountCalls==0);CHECK(capacityBytes());CHECK(requestFormat(Format::Fat,connectionToken()));serviceWorker();run();CHECK(rawCalls==1);CHECK(!lastFat32);CHECK(mounted());
 clear();connect();failTask=true;CHECK(requestFormat(Format::Fat32,connectionToken()));serviceWorker();CHECK(formatState()==FormatState::Failed);CHECK(mounted());CHECK(!pending());CHECK(rawCalls==0);CHECK(retain(lease));release();
 clear();connect();failFormat=true;CHECK(requestFormat(Format::Fat32,connectionToken()));serviceWorker();run();CHECK(formatState()==FormatState::Failed);CHECK(!mounted());CHECK(!pending());
 clear();connect();syncError=ESP_FAIL;CHECK(requestFormat(Format::Fat32,connectionToken()));serviceWorker();run();CHECK(rawCalls==0);CHECK(mounted());CHECK(formatState()==FormatState::Failed);
 clear();connect();sdConflict=true;CHECK(requestFormat(Format::Fat32,connectionToken()));serviceWorker();run();CHECK(rawCalls==1);CHECK(!mounted());CHECK(formatState()==FormatState::Failed);
 clear();sectors=8U*1024*1024;connect();CHECK(!supported(Format::Fat));CHECK(supported(Format::Fat32));CHECK(!requestFormat(Format::Fat,connectionToken()));CHECK(rawCalls==0);
 clear();sectors=2048;connect();CHECK(supported(Format::Fat));CHECK(!supported(Format::Fat32));CHECK(!requestFormat(Format::Fat32,connectionToken()));
 for(uint32_t ss:{0u,1u,511u,513u,8192u})CHECK(!Tab5UsbFormatPolicy::geometry(Format::Fat32,131072,ss));
 CHECK(!Tab5UsbFormatPolicy::geometry(static_cast<Format>(99),131072,512));
 // A UI observer which read the ready note during worker teardown gets a
 // second event after pending is false, so its failed/disabled view can retry.
 clear();connect();CHECK(requestFormat(Format::Fat32,connectionToken()));serviceWorker();uint32_t finishingRevision=0;
 onCheckpoint=[&](){CHECK(mounted());CHECK(pending());finishingRevision=revision();};run();
 CHECK(!pending());CHECK(revision()!=finishingRevision);CHECK(retain(lease));release();
 // Manual eject/remount remains usable after formatting, without OS restart.
 eject();serviceWorker();CHECK(!mounted());mount();serviceWorker();CHECK(mounted()&&!pending());CHECK(retain(lease));release();
 clear();for(unsigned i=0;i<20000;++i)serviceWorker();CHECK(rawCalls==0);CHECK(mountCalls==0);CHECK(!queuedTask);vQueueDelete(events);events=nullptr;
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
}
