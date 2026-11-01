#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <unordered_set>
#include "Tab5AppApi.h"
// The actual loader/lifecycle functions are included below. Only the hardware,
// filesystem, validator/relocator, admission and scheduler boundaries are fake.
using TaskHandle_t=void*;
using BaseType_t=int;
using TaskFunction_t=void(*)(void*);
constexpr int pdPASS=1,pdFAIL=0,MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,FILE_READ=0;
constexpr unsigned portMAX_DELAY=~0u;
constexpr size_t ConsoleCapacity=8192,StackBytes=16384,MaxPath=260,MaxArgument=511;
enum class Fault{None,Storage,Open,ShortRead,Validate,ReadAdmission,ImageAdmission,ContextStorage,Relocate,Sync,ExternalTask,InitialTask,CancelRead,CancelValidate,CancelSync};
Fault fault=Fault::None;
bool ui=true,immediateService=true,external=false;
unsigned cleanupCalls=0,entryCalls=0,readCalls=0,openCalls=0,allocationCalls=0,storageCalls=0,leases=0,files=0;
unsigned delayCalls=0,nativeServices=0,externalDeletes=0,failAllocation=0;
size_t fileBytes=65536;
std::unordered_set<void*> allocations;
void tab5ExecutableService();
const char* tab5ExecutableConsoleSnapshot();
void tab5ExecutableClearOutput();
bool tab5ExecutableBusy();
void uiService(){bool saved=ui;ui=true;tab5ExecutableService();ui=saved;}
struct Flag {
 std::atomic<bool> value{false};bool onTrue=false,onFalse=false;
 bool load(std::memory_order order=std::memory_order_seq_cst)const{return value.load(order);}
 operator bool()const{return load();}
 void store(bool next,std::memory_order order=std::memory_order_seq_cst){value.store(next,order);if(immediateService&&((next&&onTrue)||(!next&&onFalse)))uiService();}
 void operator=(bool next){store(next);}
 bool exchange(bool next,std::memory_order order){return value.exchange(next,order);}
} launching,released;
std::atomic<bool> running{false},cancelled{false},loadFailed{false};
std::atomic<const char*> status{"Prêt"};
std::atomic<uint32_t> revision{0};
bool registered=false,memoryCheckPending=false,launchErrorPending=false;
uint32_t memoryCheckAt=0,snapshotRevision=UINT32_MAX;
char sourcePath[MaxPath+1]{},*console=nullptr,*snapshot=nullptr;
size_t consoleLength=0;
struct LaunchRequest {char path[MaxPath+1];char argument[MaxArgument+1];char language[3];};
unsigned millis(){return 123;}
void* heap_caps_malloc(size_t size,int){if(++allocationCalls==failAllocation)return nullptr;void* p=std::malloc(size);if(p)allocations.insert(p);return p;}
void* heap_caps_calloc(size_t count,size_t size,int caps){void* p=heap_caps_malloc(count*size,caps);if(p)std::memset(p,0,count*size);return p;}
void heap_caps_free(void* p){if(p){assert(allocations.erase(p)==1);std::free(p);}}
struct Tab5StorageUse {
 bool active=false;
 explicit Tab5StorageUse(bool){++storageCalls;active=!(fault==Fault::Storage&&storageCalls==1)&&!(fault==Fault::ContextStorage&&storageCalls==2);if(active)++leases;}
 ~Tab5StorageUse(){if(active){assert(leases);--leases;}}
 explicit operator bool()const{return active;}
};
void probeUi(){
 assert(!ui&&launching.load());unsigned before=nativeServices;uiService();assert(nativeServices==before);assert(tab5ExecutableBusy());
 assert(!*tab5ExecutableConsoleSnapshot());tab5ExecutableClearOutput();
}
struct File {
 bool opened=false;
 File()=default;
 explicit File(bool valid):opened(valid){if(opened)++files;}
 File(File&& other):opened(other.opened){other.opened=false;}
 File(const File&)=delete;
 ~File(){close();}
 explicit operator bool()const{return opened;}
 bool isDirectory()const{return false;}
 size_t size()const{return fileBytes;}
 size_t read(void* data,size_t count){assert(!ui&&opened);++readCalls;probeUi();if(fault==Fault::CancelRead)cancelled=true;if(fault==Fault::ShortRead)return count-1;std::memset(data,0,count);return count;}
 void close(){if(opened){assert(files);--files;opened=false;}}
};
struct Filesystem {File open(const char*,int){assert(!ui);++openCalls;return File(fault!=Fault::Open);}} tab5FS;
struct Writer{void abort(Filesystem&){assert(ui);}};
struct Elf{void* psegment=nullptr;};
struct Allocation{void* pointer=nullptr;};
struct Context{
 bool externalStack=false;std::atomic<bool> startReady{false};Tab5StorageUse storage{true};Elf elf;
 tab5_app_api api{};tab5_app_api2 api2{};tab5_app_api3 api3{};Writer imageWrite;
 TaskHandle_t worker=nullptr;unsigned lastCooperation=0;char argument[MaxArgument+1]{},language[3]{'e','n',0};
 int result=0;bool audio=false;File files[TAB5_APP_MAX_FILES];Allocation allocations[32];
 ~Context(){assert(ui);++cleanupCalls;}
};
Context* context=nullptr;
namespace Tab5Resources {
enum class Owner{Tools};
struct Budget{size_t psram=0,psramBlock=0,internal=0;};
Budget TaskBudget(size_t n){Budget b;b.internal=n;return b;}
struct Reservation{bool valid=false;Reservation()=default;explicit Reservation(bool v):valid(v){}explicit operator bool()const{return valid;}void allocated(size_t,size_t){}void release(){valid=false;}};
}
Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Budget budget,const char* name){
 if(!std::strcmp(name,"chargement ELF")){assert(ui&&budget.internal==StackBytes&&budget.psram==sizeof(LaunchRequest));}
 if(!std::strcmp(name,"lecture ELF")){assert(!ui&&budget.psram==fileBytes);if(fault==Fault::ReadAdmission)return {};}
 if(!std::strcmp(name,"application")){assert(!ui);assert(budget.internal==(external?2048u:0u));if(fault==Fault::ImageAdmission)return {};}
 return Tab5Resources::Reservation(true);
}
namespace Tab5ElfValidate{
constexpr size_t MaxFile=4*1024*1024;
struct Image{size_t bytes=65536,stack=StackBytes;};
bool validate(const uint8_t*,size_t,Image& image,const char** reason,bool(*yield)()){
 assert(!ui);probeUi();image.stack=external?32768:StackBytes;
 if(fault==Fault::CancelValidate)cancelled=true;
 if(!yield())return false;
 if(fault==Fault::Validate){*reason="injected invalid ELF";return false;}return true;
}}
struct Debug{template<class... T>void printf(const char*,T...){}}Tab5DebugSerial;
void tab5StorageFault(const char*,const char*,int){assert(!ui);}
void tab5LogMemory(const char*){assert(ui);}
void tab5TaskCheckpoint(const char*){assert(!ui);}
int consoleLock(){return 1;}
void xSemaphoreTake(int,unsigned){}
void xSemaphoreGive(int){}
void registerCache(){assert(ui);registered=true;}
bool tab5LaunchPath(const char* p,char* result,size_t n){if(!p||std::strlen(p)>=n)return false;std::strcpy(result,p);return true;}
namespace Tab5Hardware {enum class Owner{Application};void release(Owner){}}
namespace Tab5NativeView {void service(){assert(ui);++nativeServices;}void close(){assert(ui);}}
namespace Tab5AppPanel {void detach(){assert(ui);}}
enum class Tab5AudioClient{None,Native};
struct Audio {void stop(Tab5AudioClient){}void release(Tab5AudioClient){}Tab5AudioClient owner(){return Tab5AudioClient::None;}}Tab5AudioDevice;
TaskFunction_t initialFunction=nullptr,externalFunction=nullptr;
void *initialArgument=nullptr,*externalArgument=nullptr,*externalStack=nullptr;
int handleToken;
enum TaskState{eRunning,eBlocked,eSuspended};
TaskState taskState=eBlocked;
TaskHandle_t xTaskGetCurrentTaskHandle(){assert(!ui);return &handleToken;}
TaskState eTaskGetState(TaskHandle_t h){assert(ui&&h==&handleToken);return taskState;}
void vTaskDelay(int){assert(!ui);++delayCalls;}
struct Suspended{};
void vTaskSuspend(void*){assert(!ui&&context&&!context->startReady.load());taskState=eSuspended;throw Suspended{};}
void vTaskDeleteWithCaps(TaskHandle_t h){assert(ui&&h==&handleToken&&taskState==eSuspended);++externalDeletes;heap_caps_free(externalStack);externalStack=nullptr;}
BaseType_t tab5CreateAdmittedTask(Tab5Resources::Reservation&,TaskFunction_t f,const char*,size_t stack,void* arg,unsigned priority,std::nullptr_t,int core){assert(ui&&stack==StackBytes&&priority==2&&core==0);if(fault==Fault::InitialTask)return pdFAIL;initialFunction=f;initialArgument=arg;return pdPASS;}
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t f,const char*,size_t stack,void* arg,unsigned priority,TaskHandle_t* handle,int core,int){
 assert(!ui&&stack==32768&&priority==2&&core==0);if(fault==Fault::ExternalTask)return pdFAIL;
 externalStack=heap_caps_malloc(stack,0);if(!externalStack)return pdFAIL;externalFunction=f;externalArgument=arg;*handle=&handleToken;return pdPASS;
}
void esp_elf_init(Elf*){assert(!ui);}
int symbols;
void esp_elf_register_symbol(int){assert(!ui);}
int esp_elf_relocate(Elf* elf,const uint8_t*){assert(!ui);probeUi();elf->psegment=heap_caps_malloc(65536,0);if(!elf->psegment)return -ENOMEM;return fault==Fault::Relocate?-1:0;}
void esp_elf_deinit(Elf* elf){assert(ui);heap_caps_free(elf->psegment);elf->psegment=nullptr;}
int tab5_elf_sync(Elf*,size_t){assert(!ui);probeUi();if(fault==Fault::CancelSync)cancelled=true;return fault==Fault::Sync?-1:0;}
int esp_elf_request(Elf* elf,int,int argc,char** argv){assert(!ui&&elf==&context->elf&&argc==2&&!std::strcmp(argv[1],"/Pictures/test.png")&&!std::strcmp(context->language,"fr"));++entryCalls;assert(!launching.load());return 0;}
// Actual ABI declarations type-check the preserved API construction in loader.
void (*appLog)(const char*,size_t)=nullptr;int (*appStop)()=nullptr;int (*appSleep)(uint32_t)=nullptr;
void* (*appAllocate)(size_t)=nullptr;void (*appRelease)(void*)=nullptr;int (*appOpen)(const char*)=nullptr;
int (*appRead)(int,void*,size_t)=nullptr;int (*appSeek)(int,uint32_t)=nullptr;int (*appClose)(int)=nullptr;
uint32_t (*appMillis)()=nullptr;uint32_t (*appSize)(int)=nullptr;int (*appVideoBegin)(uint16_t,uint16_t)=nullptr;
int (*appPresent)(const uint16_t*,size_t)=nullptr;uint32_t (*appButtons)()=nullptr;int (*appAudio)(const int16_t*,size_t,uint32_t)=nullptr;
int (*appSave)(const char*,const void*,size_t)=nullptr;int (*appHardware)(int,int,int,int,const void*,size_t,void*,size_t)=nullptr;
#include "elf-loader-production.inc"

void reset(Fault next,bool useExternal,unsigned allocationFailure){
 assert(allocations.empty()&&!context&&!leases&&!files&&!externalStack);
 fault=next;external=useExternal;ui=true;cleanupCalls=entryCalls=readCalls=openCalls=allocationCalls=storageCalls=delayCalls=nativeServices=externalDeletes=0;failAllocation=allocationFailure;
 launching.value=false;released.value=false;launching.onFalse=true;released.onTrue=true;
 running=false;cancelled=false;loadFailed=false;launchErrorPending=false;memoryCheckPending=false;
 initialFunction=externalFunction=nullptr;initialArgument=externalArgument=nullptr;taskState=eBlocked;
}
void runCase(Fault next,bool useExternal=false,unsigned allocationFailure=0,bool stopBefore=false){
 reset(next,useExternal,allocationFailure);
 bool accepted=tab5ExecutableRun("/Apps/Test.app.elf","/Pictures/test.png","fr");
 assert(!openCalls&&!readCalls); // No SD or validation before UI returns.
 if(!accepted){assert(next==Fault::InitialTask||allocationFailure==1);assert(!tab5ExecutableBusy()&&!initialFunction);}
 else {
  assert(tab5ExecutableBusy()&&launching.load()&&initialFunction);assert(!tab5ExecutableRun("/Apps/Other.app.elf","","en"));
  if(stopBefore)tab5ExecutableStop();
  ui=false;initialFunction(initialArgument);ui=true;
  if(externalFunction){assert(context&&context->startReady.load()&&leases==1);ui=false;try{externalFunction(externalArgument);assert(false);}catch(const Suspended&){}ui=true;tab5ExecutableService();assert(externalDeletes==1);}
  tab5ExecutableService();assert(!tab5ExecutableBusy()&&!context&&!running.load());
 }
 const bool stopped=stopBefore||next==Fault::CancelRead||next==Fault::CancelValidate||next==Fault::CancelSync;
 const bool success=accepted&&next==Fault::None&&!allocationFailure&&!stopped;
 assert(entryCalls==unsigned(success));assert(!leases&&!files&&!externalStack);
 bool error=tab5ExecutableTakeLaunchError();assert(error==(accepted&&!success&&!stopped));assert(!tab5ExecutableTakeLaunchError());
 if(stopped)assert(!std::strcmp(status.load(),"Application arrêtée"));
 if(console)heap_caps_free(console);if(snapshot)heap_caps_free(snapshot);console=snapshot=nullptr;consoleLength=0;snapshotRevision=UINT32_MAX;
 assert(allocations.empty());
}
int main(){
 unsigned cases=0;
 for(unsigned repeat=0;repeat<32;++repeat){
  runCase(Fault::None);runCase(Fault::None,true);runCase(Fault::None,false,0,true);cases+=3;
  for(Fault f:{Fault::Storage,Fault::Open,Fault::ShortRead,Fault::Validate,Fault::ReadAdmission,Fault::ImageAdmission,Fault::ContextStorage,Fault::Relocate,Fault::Sync,Fault::InitialTask,Fault::CancelRead,Fault::CancelValidate,Fault::CancelSync}){runCase(f);++cases;}
  runCase(Fault::ExternalTask,true);++cases;
  for(unsigned allocation=1;allocation<=6;++allocation){runCase(Fault::None,false,allocation);++cases;}
  runCase(Fault::None,true,7);++cases;
 }
 fileBytes=4*1024*1024;runCase(Fault::None);++cases;assert(delayCalls==257&&readCalls==1024);
 std::printf("%u loader cases: deferred SD/validation, cancel before/during load, partial allocation and relocation failures, UI-only cleanup, external stack handoff, preserved status, no outstanding tracked allocations/files/leases; 4 MiB read yields 256 times on worker\n",cases);
}
