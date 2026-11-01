#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static unsigned checks=0,cycles=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(2);}}while(0)
using TaskHandle_t=void*;
enum State{eRunning,eBlocked,eSuspended};
static State taskState=eRunning;
static int handleStorage;
static bool elfActive=false,audioActive=false,hardwareActive=false,checkpoint=false;
static bool capsDeleted=false,immediateCleanup=false,interleaveSuspend=false;
static unsigned cleanups=0,requests=0,taskDeletes=0;
static unsigned audioReleaseDelays=0,audioReleaseCalls=0;
static bool audioOwned=false,memoryCheckPending=false,launchErrorPending=false;
static std::atomic<bool> launching{false},loadFailed{false};
static uint32_t memoryCheckAt=0;
static int resultCode=0;
void tab5ExecutableService();
struct ReleaseFlag{
 std::atomic<bool> flag{false};
 void store(bool value,std::memory_order order){flag.store(value,order);if(value&&immediateCleanup){immediateCleanup=false;tab5ExecutableService();}}
 bool exchange(bool value,std::memory_order order){return flag.exchange(value,order);}
}released;
std::atomic<bool> running{false},cancelled{false};
std::atomic<const char*> status{"ready"};
struct Context{bool externalStack=false;std::atomic<bool>startReady{false};TaskHandle_t worker=nullptr;unsigned lastCooperation=0;char argument[64]{};int result=0,elf=123;bool audio=false;};
Context* context=nullptr;
char sourcePath[]="/sd/test.elf";
namespace Tab5Hardware{enum class Owner{Application};void release(Owner){CHECK(!elfActive);hardwareActive=false;}}
enum class Tab5AudioClient{None,Native};
struct Audio{void stop(Tab5AudioClient){CHECK(audioActive);audioActive=false;}void release(Tab5AudioClient){CHECK(!audioActive);++audioReleaseCalls;if(!audioReleaseDelays)audioOwned=false;}Tab5AudioClient owner(){return audioOwned?Tab5AudioClient::Native:Tab5AudioClient::None;}}Tab5AudioDevice;
namespace Tab5NativeView{void service(){}}
TaskHandle_t xTaskGetCurrentTaskHandle(){return &handleStorage;}
void vTaskDelay(int){CHECK(audioReleaseDelays>0&&audioOwned&&!audioActive&&context);--audioReleaseDelays;CHECK(running.load()&&!released.flag.load()&&cleanups==0);}
unsigned millis(){return 1;}
void tab5LogMemory(const char*){}
void tab5TaskCheckpoint(const char*){CHECK(!elfActive&&!audioActive&&!audioOwned&&!hardwareActive);checkpoint=true;}
int esp_elf_request(int* elf,int,int argc,char**argv){CHECK(context&&elf==&context->elf);CHECK(argc==(context->argument[0]?2:1));CHECK(argv[0]==sourcePath);CHECK((argc==1&&!argv[1])||(argc==2&&argv[1]==context->argument));++requests;elfActive=true;elfActive=false;return resultCode;}
State eTaskGetState(TaskHandle_t handle){CHECK(handle==&handleStorage);return taskState;}
struct Suspended{};
void vTaskSuspend(TaskHandle_t handle){CHECK(!handle);CHECK(context&&context->externalStack&&!context->startReady.load());CHECK(checkpoint);if(interleaveSuspend){taskState=eRunning;tab5ExecutableService();CHECK(context&&cleanups==0);taskState=eBlocked;tab5ExecutableService();CHECK(context&&cleanups==0);}taskState=eSuspended;throw Suspended{};}
void vTaskDeleteWithCaps(TaskHandle_t handle){CHECK(handle==&handleStorage);CHECK(taskState==eSuspended);CHECK(context&&context->externalStack);CHECK(!elfActive&&!audioActive&&!hardwareActive&&checkpoint);capsDeleted=true;++taskDeletes;}
void cleanup(){CHECK(context);CHECK(!elfActive&&!audioActive&&!hardwareActive&&checkpoint);CHECK(!context->externalStack||capsDeleted);if(context->externalStack)CHECK(context->worker==nullptr);++cleanups;delete context;context=nullptr;}
#include "elf-task-production.inc"
#include "elf-service-production.inc"
int main(){
 for(int repeat=0;repeat<128;++repeat)for(int ext=0;ext<2;++ext)for(int cancel=0;cancel<2;++cancel)for(int audio=0;audio<2;++audio){
  ++cycles;context=new Context;context->externalStack=ext;if(ext)context->worker=&handleStorage;context->startReady.store(true);context->audio=audio;if(repeat%2)std::strcpy(context->argument,"/sd/test.ext");
  released.flag=false;running=true;cancelled=cancel;elfActive=false;audioActive=audio;hardwareActive=true;checkpoint=false;capsDeleted=false;cleanups=0;requests=0;taskDeletes=0;taskState=eRunning;resultCode=repeat%3;
  immediateCleanup=!ext;interleaveSuspend=ext;audioOwned=audio;audioReleaseDelays=audio?repeat%4:0;const unsigned expectedReleaseCalls=audio?audioReleaseDelays+1:0;audioReleaseCalls=0;
  if(ext){try{task(context);CHECK(false);}catch(const Suspended&){}CHECK(context&&running.load()&&cleanups==0&&!released.flag.load());tab5ExecutableService();CHECK(taskDeletes==1&&capsDeleted);}
  else{task(context);CHECK(taskDeletes==0);}
  CHECK(requests==unsigned(!cancel));CHECK(!context&&!running.load());CHECK(cleanups==1);CHECK(!released.flag.load());
  CHECK(!audioOwned&&!audioReleaseDelays&&audioReleaseCalls==expectedReleaseCalls);
  const char* expected=cancel?"Application arrêtée":resultCode?"Erreur de l'application — voir la console":"Application terminée";
  // A cancelled worker does not execute the entry point, hence its result is 0.
  CHECK(std::strcmp(status.load(),expected)==0);
  tab5ExecutableService();CHECK(cleanups==1);
 }
 std::printf("{\"passed\":true,\"checks\":%u,\"cycles\":%u,\"immediateContextCleanup\":true,\"capsSuspendDeleteOrdering\":true,\"audioReleaseContention\":true}\n",checks,cycles);
}
