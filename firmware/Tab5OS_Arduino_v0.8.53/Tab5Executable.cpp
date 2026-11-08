#include "Tab5Serial.h"
#include "Tab5Environment.h"
#include "Tab5Hardware.h"
#include "Tab5VolumeFS.h"
#include "Tab5Executable.h"
#include "Tab5LaunchProfile.h"
#include <freertos/idf_additions.h>
#include "Tab5AppApi.h"
#include "Tab5ImageWrite.h"
#include "Tab5AppPanel.h"
#include "Tab5NativeView.h"
#include "Tab5AudioDriver.h"
#include "Tab5ElfValidate.h"
#include "Tab5ResourceManager.h"
#include "Tab5Storage.h"
#include "Tab5AtomicFile.h"
#include "Tab5Task.h"
#include "Tab5Memory.h"
#include "src/tab5_elf/esp_elf.h"
#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <atomic>
#include <new>
#include <cstring>
#include <algorithm>
#include <cmath>
extern "C" int tab5_elf_sync(esp_elf_t*,size_t);
namespace {
using ImageWriter=Tab5ImageWrite<fs::FS,File>;
constexpr size_t ConsoleCapacity=8192,StackBytes=16384,MaxPath=260,MaxArgument=511;
std::atomic<bool> running{false},cancelled{false},released{false},launching{false},loadFailed{false};
std::atomic<const char*> status{"Prêt"};
std::atomic<uint32_t> revision{0};
char sourcePath[MaxPath+1]{};
char* console=nullptr;char* snapshot=nullptr;size_t consoleLength=0;
uint32_t snapshotRevision=UINT32_MAX;bool registered=false,launchErrorPending=false;
struct LaunchRequest {char path[MaxPath+1];char argument[MaxArgument+1];char language[3];};
bool memoryCheckPending=false;uint32_t memoryCheckAt=0;
StaticSemaphore_t consoleLockStorage;
SemaphoreHandle_t consoleLock(){static auto lock=xSemaphoreCreateMutexStatic(&consoleLockStorage);return lock;}
struct Allocation {void* pointer=nullptr;size_t bytes=0;};
struct Context {bool externalStack=false;std::atomic<bool> startReady{false};
 Tab5StorageUse storage{true};
 esp_elf_t elf{};
 tab5_app_api api{};tab5_app_api2 api2{};tab5_app_api3 api3{};tab5_app_api4 api4{};tab5_app_api5 api5{};tab5_app_api6 api6{};unsigned abi=1;
 ImageWriter imageWrite;
 bool audio=false,extended=false,panel=false;
 TaskHandle_t worker=nullptr;
 File files[TAB5_APP_MAX_FILES];
 Allocation allocations[32];
 size_t allocated=0;
 uint32_t lastCooperation=0;
 char argument[MaxArgument+1]{};char language[3]{'e','n',0};
 int result=0;
};
Context* context=nullptr;
bool worker(){return context&&context->worker==xTaskGetCurrentTaskHandle();}
// Polling for input/cancellation or presenting frames is not a FreeRTOS
// yield. Bound consecutive CPU time at the ABI boundary, including loading
// loops that repeatedly read from SD. Idle and network tasks must really run;
// resetting/disabling their watchdog would only hide their starvation.
bool cooperate(){
 if(!worker())return false;
 if(uint32_t(millis()-context->lastCooperation)>=8U){
  vTaskDelay(1);context->lastCooperation=millis();
 }
 return true;
}
void append(const char* bytes,size_t length){
 if(!bytes||!length)return;
 length=std::min(length,size_t(TAB5_APP_MAX_IO));
 xSemaphoreTake(consoleLock(),portMAX_DELAY);
 if(console){
  if(consoleLength+length>=ConsoleCapacity){size_t drop=consoleLength+length-ConsoleCapacity+1;memmove(console,console+drop,consoleLength-drop);consoleLength-=drop;}
  // Embedded NUL would hide all following output from the UI.
  for(size_t i=0;i<length;++i)console[consoleLength+i]=bytes[i]?bytes[i]:' ';
  consoleLength+=length;console[consoleLength]=0;revision.fetch_add(1,std::memory_order_release);
 }
 xSemaphoreGive(consoleLock());
}
void appLog(const char* bytes,size_t length){if(cooperate())append(bytes,length);}
bool panelClosing(){return context&&context->panel&&Tab5AppPanel::closing();}
int appStop(){return !cooperate()||cancelled.load(std::memory_order_acquire)||panelClosing();}
int appSleep(uint32_t milliseconds){
 if(!worker())return 1;
 const uint32_t begin=millis();
 do {if(cancelled.load()||panelClosing())return 1;vTaskDelay(1);}while(uint32_t(millis()-begin)<milliseconds);
 return cancelled.load()||panelClosing()?1:0;
}
void* appAllocate(size_t bytes){
 if(!cooperate()||!bytes||bytes>(context->abi>=4?16u*1024u*1024u:context->extended?4u*1024u*1024u:TAB5_APP_MAX_ALLOCATION)||bytes>(context->abi>=4?20u*1024u*1024u:context->extended?8u*1024u*1024u:TAB5_APP_MAX_HEAP)-context->allocated)return nullptr;
 const size_t rounded=(bytes+15U)&~size_t(15U);
 if(rounded>(context->abi>=4?20u*1024u*1024u:context->extended?8u*1024u*1024u:TAB5_APP_MAX_HEAP)-context->allocated)return nullptr;
 Allocation* slot=nullptr;for(auto& item:context->allocations)if(!item.pointer){slot=&item;break;}if(!slot)return nullptr;
 Tab5Resources::Budget budget{};budget.psram=rounded;budget.psramBlock=rounded;
 auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,budget,"application ELF: allocation");
 if(!admission)return nullptr;
 // The bundled C runtime places alignas(16) allocator headers here.
 // Do not rely on the default heap alignment for application-owned arenas.
 void* p=heap_caps_aligned_alloc(16,rounded,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!p)return nullptr;
 slot->pointer=p;slot->bytes=rounded;context->allocated+=rounded;admission.allocated(0,rounded);return p;
}
void appRelease(void* pointer){
 if(!worker()||!pointer)return;
 for(auto& item:context->allocations)if(item.pointer==pointer){heap_caps_free(pointer);context->allocated-=item.bytes;item={};return;}
}
// Fixed-size path parser: no unbounded strlen, dynamic strings or traversal.
bool validPath(const char* path){
 if(!path||path[0]!='/')return false;size_t n=strnlen(path,MaxPath+1);if(n<2||n>MaxPath||path[n-1]=='/')return false;
 size_t start=1;
 for(size_t i=1;i<=n;++i){unsigned char c=path[i];
  if(i==n||c=='/'){
   size_t count=i-start;if(!count||(count==1&&path[start]=='.')||(count==2&&path[start]=='.'&&path[start+1]=='.')||path[i-1]==' '||path[i-1]=='.')return false;
   start=i+1;
  }else if(c<32||c==127||strchr("\\:*?\"<>|~",c))return false;
 }return true;
}
int appOpen(const char* path){
 if(!cooperate()||cancelled.load()||!validPath(path))return -1;
 if(ImageWriter::valid(path)&&!ImageWriter::recover(tab5FS,path))return -1;
 for(unsigned i=0;i<TAB5_APP_MAX_FILES;++i)if(!context->files[i]){
  File f=tab5FS.open(path,FILE_READ);if(!f||f.isDirectory()){f.close();return -1;}context->files[i]=f;return int(i+1);
 }return -1;
}
File* appFile(int handle){return worker()&&handle>0&&handle<=int(TAB5_APP_MAX_FILES)&&context->files[handle-1]?&context->files[handle-1]:nullptr;}
int appRead(int handle,void* destination,size_t capacity){
 if(!cooperate())return -1;
 File* file=appFile(handle);if(!file||cancelled.load()||!destination||capacity>TAB5_APP_MAX_IO)return -1;
 if(!capacity)return 0;size_t position=file->position(),length=file->size();if(position>=length)return 0;
 size_t wanted=std::min(capacity,length-position),got=file->read(static_cast<uint8_t*>(destination),wanted);
 if(got!=wanted){tab5StorageFault("Lecture application ELF",sourcePath,EIO);return -1;}return int(got);
}
int appSeek(int handle,uint32_t offset){if(!cooperate())return -1;File* file=appFile(handle);return file&&!cancelled.load()&&offset<=file->size()&&file->seek(offset)?0:-1;}
int appClose(int handle){File* file=appFile(handle);if(!file)return -1;file->close();return 0;}
uint32_t appMillis(){return cooperate()?millis():0;}
const tab5_app_api* appApi(){return worker()?&context->api:nullptr;}
uint32_t appSize(int handle){auto* f=appFile(handle);return f?uint32_t(f->size()):0;}
int appVideoBegin(uint16_t w,uint16_t h){return worker()&&!cancelled.load()&&Tab5NativeView::begin(w,h)?0:-1;}
int appPresent(const uint16_t* pixels,size_t count){return cooperate()&&!cancelled.load()&&Tab5NativeView::present(pixels,count)?0:-1;}
uint32_t appButtons(){return cooperate()?Tab5NativeView::buttons():0;}
int appAudio(const int16_t* samples,size_t frames,uint32_t rate){
 if(!cooperate()||cancelled.load()||!samples||frames>2048||rate<8000||rate>48000)return -1;
 if(!context->audio){if(!Tab5AudioDevice.claim(Tab5AudioClient::Native,0))return -1;context->audio=true;}
 Tab5PcmSpec spec;spec.sampleRate=rate;spec.channels=2;spec.format=Tab5PcmFormat::S16;
 return Tab5AudioDevice.writePcm(Tab5AudioClient::Native,samples,frames,spec)?0:-1;
}
int appSave(const char* path,const void* data,size_t bytes){
 if(!worker()||!validPath(path)||!data||!bytes||bytes>(context->abi>=4?2u*1024u*1024u:512u*1024u))return -1;
 size_t n=strlen(path);const char* ext=strrchr(path,'.');if(n<14||!ext||(strcmp(ext,".sav")&&strcmp(ext,".state")&&!(context->abi>=4&&(!strcmp(ext,".dsg")||!strcmp(ext,".cfg")))))return -1;
 bool present=false;if(!tab5AtomicProbe("/Saves",present)||(!present&&!tab5FS.mkdir("/Saves")))return -1;
 const char* folder=nullptr;const char* folders[]={"/Saves/GB","/Saves/GBC","/Saves/NES","/Saves/Sega8","/Saves/Doom","/Saves/Maze3D","/Saves/GBA","/Saves/SNES","/Saves/MegaDrive"};
 for(auto* candidate:folders){size_t n=strlen(candidate);if(!strncmp(path,candidate,n)&&path[n]=='/'&&!strchr(path+n+1,'/')){folder=candidate;break;}}if(!folder)return -1;
 if(!tab5AtomicProbe(folder,present)||(!present&&!tab5FS.mkdir(folder)))return -1;
 char temporary[MaxPath+8],backup[MaxPath+8];snprintf(temporary,sizeof(temporary),"%s.tmp",path);snprintf(backup,sizeof(backup),"%s.old",path);
 // A previous interruption may leave the old save waiting for recovery.
 if(!tab5AtomicProbe(path,present))return -1;
 if(!present){if(!tab5AtomicProbe(backup,present)||(present&&!tab5FS.rename(backup,path)))return -1;}
 // An unreadable primary/backup is not an absent save. Never truncate a
 // stale temporary until its existence and removal are confirmed.
 if(!tab5AtomicProbe(temporary,present)||(present&&!tab5FS.remove(temporary)))return -1;
 File f=tab5FS.open(temporary,FILE_WRITE);if(!f)return -1;
 size_t offset=0;for(;offset<bytes;){size_t count=std::min(size_t(4096),bytes-offset);if(f.write(static_cast<const uint8_t*>(data)+offset,count)!=count)break;offset+=count;vTaskDelay(1);}
 errno=0;f.flush();const bool synced=!errno;bool ok=synced&&offset==bytes&&!f.getWriteError()&&f.size()==bytes;errno=0;f.close();ok=ok&&!errno;if(!ok){tab5StorageFault("Sauvegarde jeu",path,EIO);tab5FS.remove(temporary);return -1;}
 if(!tab5AtomicReplace(temporary,path,backup)){tab5StorageFault("Sauvegarde jeu",path,errno?errno:EIO);return -1;}
 // The new save is committed. Preserve the previous cleanup convention;
 // failure to remove an old backup does not invalidate the committed save.
 tab5FS.remove(backup);return 0;
}
const tab5_app_api2* appApi2(){if(!worker())return nullptr;context->extended=true;return &context->api2;}
int appHardware(int op,int a,int b,int c,const void* in,size_t n,void* out,size_t capacity){if(!cooperate())return -EPERM;if(cancelled.load()&&op!=7&&op!=9&&op!=13&&op!=22&&op!=33)return -ECANCELED;if(op>=107&&op<=109)return tab5_env_call(op,a,in,n,out,capacity);return Tab5Hardware::call(Tab5Hardware::Owner::Application,op,a,b,c,in,n,out,capacity);}
const tab5_app_api3* appApi3(){if(!worker())return nullptr;context->extended=true;return &context->api3;}
uint64_t appFloating(unsigned op,uint64_t a,uint64_t b){
 if(!worker())return 0;union D{uint64_t bits;double value;};union F{uint32_t bits;float value;};D x{a},y{b},z{};F f{uint32_t(a)};
 switch(op){case 0:z.value=x.value+y.value;break;case 1:z.value=x.value-y.value;break;case 2:z.value=x.value*y.value;break;case 3:z.value=x.value/y.value;break;
 case 4:z.value=int32_t(a);break;case 5:z.value=uint32_t(a);break;case 6:z.value=int64_t(a);break;case 7:z.value=a;break;case 8:z.value=f.value;break;case 9:f.value=float(x.value);return f.bits;
 case 10:return int32_t(x.value);case 11:return uint32_t(x.value);case 12:return int64_t(x.value);case 13:return uint64_t(x.value);
 case 20:z.value=sin(x.value);break;case 21:z.value=cos(x.value);break;case 22:z.value=tan(x.value);break;case 23:z.value=atan(x.value);break;case 24:z.value=atan2(x.value,y.value);break;case 25:z.value=acos(x.value);break;case 26:z.value=asin(x.value);break;case 27:z.value=sqrt(x.value);break;case 28:z.value=floor(x.value);break;case 29:z.value=ceil(x.value);break;case 30:z.value=fmod(x.value,y.value);break;case 31:z.value=pow(x.value,y.value);break;case 32:z.value=exp(x.value);break;case 33:z.value=log(x.value);break;case 34:z.value=ldexp(x.value,int32_t(b));break;
 default:return 0;}return z.bits;
}
const tab5_app_api4* prepareApi4(bool controls){if(!worker())return nullptr;context->extended=true;context->abi=4;if(controls)Tab5NativeView::extendedControls(true);context->api4.base=context->api3;context->api4.floating=appFloating;context->api4.calendar=[](uint32_t value,int utc,int* out)->int{if(!worker()||!out)return -1;time_t seconds=value;struct tm info{};if(!(utc?gmtime_r(&seconds,&info):localtime_r(&seconds,&info)))return -1;int fields[]={info.tm_sec,info.tm_min,info.tm_hour,info.tm_mday,info.tm_mon,info.tm_year,info.tm_wday,info.tm_yday,info.tm_isdst};memcpy(out,fields,sizeof(fields));return 0;};context->api4.unix_time=[]()->uint32_t{return worker()?uint32_t(time(nullptr)):0;};context->api4.base.base.base.version=4;context->api4.base.base.base.size=sizeof(tab5_app_api4);return &context->api4;}
const tab5_app_api4* appApi4(){return prepareApi4(true);}
const tab5_app_api5* appApi5(){
 auto* base=prepareApi4(false);if(!base)return nullptr;
 context->abi=5;context->panel=true;auto& api=context->api5;api.base=*base;
 api.base.base.base.base.version=5;api.base.base.base.base.size=sizeof(api);
 api.panel_open=[](const tab5_app_panel_spec* spec)->int{return !cooperate()?-EPERM:cancelled.load()?-ECANCELED:Tab5AppPanel::open(spec);};
 api.panel_poll=[](tab5_app_panel_event* event)->int{return cooperate()?Tab5AppPanel::poll(event):-EPERM;};
 api.panel_write=[](const char* bytes,size_t length,int reset)->int{return cooperate()?Tab5AppPanel::write(bytes,length,reset):-EPERM;};
 api.panel_state=[](uint32_t state)->int{return cooperate()?Tab5AppPanel::state(state):-EPERM;};
 api.network=[](int argc,const char* const* argv,void(*output)(void*,const char*,size_t),void* user)->int{
  if(!cooperate())return -EPERM;
  return Tab5AppPanel::network(argc,argv,output,user,[]()->bool{return cancelled.load(std::memory_order_acquire);});
 };
 api.translate=[](const char* key,char* output,size_t capacity)->size_t{return cooperate()?Tab5AppPanel::translate(key,output,capacity):0;};
 return &api;
}
const tab5_app_api6* appApi6(){
 auto* base=appApi5();if(!base)return nullptr;
 context->abi=6;context->panel=false;auto& api=context->api6;api.base=*base;
 api.base.panel_open=[](const tab5_app_panel_spec* spec)->int{if(!cooperate()||cancelled.load())return -1;int result=Tab5AppPanel::open(spec);if(result==0)context->panel=true;return result;};
 api.base.base.base.base.base.version=6;api.base.base.base.base.base.size=sizeof(api);
 api.canvas_begin=[](uint16_t w,uint16_t h)->int{return cooperate()&&!cancelled.load()&&Tab5NativeView::canvasBegin(w,h)?0:-1;};
 api.input_poll=[](tab5_app_input_event* event)->int{return cooperate()&&!cancelled.load()?Tab5NativeView::inputPoll(event):-1;};
 api.write_begin=[](const char* path)->int{return cooperate()&&!cancelled.load()?context->imageWrite.begin(tab5FS,path,FILE_WRITE):-1;};
 api.write_chunk=[](int handle,const void* data,size_t bytes)->int{return cooperate()&&!cancelled.load()?context->imageWrite.chunk(handle,data,bytes):-1;};
 api.write_finish=[](int handle,int commit)->int{if(!worker())return -1;bool stopped=cancelled.load();int result=context->imageWrite.finish(tab5FS,handle,commit&&!stopped);return commit&&stopped?-1:result;};
 return &api;
}
const esp_elfsym symbols[]={{"tab5_app_api_v1",reinterpret_cast<const void*>(&appApi)},{"tab5_app_api_v2",reinterpret_cast<const void*>(&appApi2)},{"tab5_app_api_v3",reinterpret_cast<const void*>(&appApi3)},{"tab5_app_api_v4",reinterpret_cast<const void*>(&appApi4)},{"tab5_app_api_v5",reinterpret_cast<const void*>(&appApi5)},{"tab5_app_api_v6",reinterpret_cast<const void*>(&appApi6)},ESP_ELFSYM_END};
void cleanup(){
 if(!context)return;
 Tab5Hardware::release(Tab5Hardware::Owner::Application);
 Tab5NativeView::close();
 Tab5AppPanel::detach();
 context->imageWrite.abort(tab5FS);
 for(auto& f:context->files)f.close();
 for(auto& item:context->allocations)if(item.pointer)heap_caps_free(item.pointer);
 esp_elf_deinit(&context->elf);
 context->~Context();heap_caps_free(context);context=nullptr;
}
size_t reclaim(void*){
 if(launching.load(std::memory_order_acquire)||context||running.load())return 0;
 xSemaphoreTake(consoleLock(),portMAX_DELAY);
 size_t freed=0;if(console){heap_caps_free(console);console=nullptr;consoleLength=0;freed+=ConsoleCapacity;revision.fetch_add(1,std::memory_order_release);}
 if(snapshot){heap_caps_free(snapshot);snapshot=nullptr;freed+=ConsoleCapacity;}snapshotRevision=UINT32_MAX;
 xSemaphoreGive(consoleLock());return freed;
}
void registerCache(){if(!registered)registered=Tab5MemoryManager.addCache(Tab5Resources::Owner::Tools,"Console applications ELF",reclaim);}
size_t cacheMissing(){return (console?0:ConsoleCapacity)+(snapshot?0:ConsoleCapacity);}
bool createCache(Tab5Resources::Reservation& admission){
 if(!console){console=static_cast<char*>(heap_caps_calloc(ConsoleCapacity,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!console)return false;admission.allocated(0,ConsoleCapacity);consoleLength=0;}
 if(!snapshot){snapshot=static_cast<char*>(heap_caps_calloc(ConsoleCapacity,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!snapshot)return false;admission.allocated(0,ConsoleCapacity);snapshotRevision=UINT32_MAX;}return true;
}
void task(void* argument){
 auto* c=static_cast<Context*>(argument);while(c->externalStack&&!c->startReady.load(std::memory_order_acquire))vTaskDelay(1);if(!c->externalStack)c->worker=xTaskGetCurrentTaskHandle();c->lastCooperation=millis();
 char* argv[]={sourcePath,c->argument[0]?c->argument:nullptr,nullptr};
 if(!cancelled.load())c->result=esp_elf_request(&c->elf,0,c->argument[0]?2:1,argv);
 Tab5Hardware::release(Tab5Hardware::Owner::Application);
 if(c->audio){
  Tab5AudioDevice.stop(Tab5AudioClient::Native);
  // release() can time out while headphone routing owns the driver mutex.
  // The entry point has returned, but its PCM buffer is still owned until the
  // release succeeds. Retry on this worker with bounded waits; never make the
  // UI wait for I2C, nor publish the context as reclaimable prematurely.
  do {Tab5AudioDevice.release(Tab5AudioClient::Native);
   if(Tab5AudioDevice.owner()==Tab5AudioClient::Native)vTaskDelay(1);
  }while(Tab5AudioDevice.owner()==Tab5AudioClient::Native);
  c->audio=false;
 }
 tab5TaskCheckpoint("application ELF");
 // WithCaps stacks must be deleted by another task. The UI waits for the
 // suspended state, calls vTaskDeleteWithCaps, then releases the context.
 if(c->externalStack){c->startReady.store(false,std::memory_order_release);for(;;)vTaskSuspend(nullptr);}
 // The ELF entry point, hardware and audio calls have all returned. Neither
 // this worker nor the dynamic-task trampoline accesses Context again. This
 // is a lifetime barrier, not a claim that the idle task reclaimed the TCB.
 // FreeRTOS TLS slot 0 belongs to the SDK pthread implementation.
 released.store(true,std::memory_order_release);
}
bool yieldValidation(){vTaskDelay(1);return !cancelled.load();}
bool loadApplication(const LaunchRequest& launch){
 const char* path=launch.path;const char* argument=launch.argument;const char* language=launch.language;
 if(cancelled.load(std::memory_order_acquire))return false;
 status="Chargement de l'application";
 Tab5StorageUse storage{true};if(!storage){status="Carte SD en cours de reconnexion";return false;}
 File file=tab5FS.open(path,FILE_READ);
 if(!file||file.isDirectory()||file.size()<52||file.size()>Tab5ElfValidate::MaxFile){status="Application inaccessible ou supérieure à 4 Mio";return false;}
 const size_t bytes=file.size();
 Tab5Resources::Budget fileBudget{};fileBudget.psram=bytes;fileBudget.psramBlock=bytes;
 auto readAdmission=tab5ReserveMemory(Tab5Resources::Owner::Tools,fileBudget,"lecture ELF");
 if(!readAdmission){status="Mémoire insuffisante pour l'application";return false;}
 auto* payload=static_cast<uint8_t*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!payload){status="Mémoire insuffisante pour l'application";return false;}readAdmission.allocated(0,bytes);readAdmission.release();
 struct PayloadFree {void* p;~PayloadFree(){heap_caps_free(p);}} payloadFree{payload};
 for(size_t offset=0;offset<bytes;){if(cancelled.load(std::memory_order_acquire))return false;size_t count=std::min(size_t(4096),bytes-offset);if(file.read(payload+offset,count)!=count){status="Lecture de l'application impossible";tab5StorageFault("Lecture ELF",path,EIO);return false;}offset+=count;if((offset&16383)==0)vTaskDelay(1);}file.close();
 Tab5ElfValidate::Image image;const char* reason=nullptr;
 if(!Tab5ElfValidate::validate(payload,bytes,image,&reason,yieldValidation)){
  Tab5DebugSerial.printf("[ELF] refuse: %s\n",reason?reason:"invalid");status="Fichier application incompatible ou endommagé";return false;
 }
 size_t imageBytes=(image.bytes+127u)&~127u;
 Tab5Resources::Reservation admission;
 // Collection may reclaim old caches during admission. Recompute if it did.
 for(unsigned attempt=0;attempt<2;++attempt){size_t missing=cacheMissing();auto budget=image.stack>StackBytes?Tab5Resources::TaskBudget(2048):Tab5Resources::Budget{};
  budget.psram=imageBytes+sizeof(Context)+missing+(image.stack>StackBytes?image.stack:0);budget.psramBlock=std::max(std::max(imageBytes,ConsoleCapacity),image.stack>StackBytes?size_t(image.stack):size_t(0));
  admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,budget,"application");
  if(!admission)break;if(missing==cacheMissing())break;admission.release();
 }
 if(!admission||!createCache(admission)){status="Mémoire insuffisante pour l'application";return false;}
 void* memory=heap_caps_malloc(sizeof(Context),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!memory){status="Mémoire insuffisante pour l'application";return false;}
 context=new(memory) Context;context->externalStack=image.stack>StackBytes;admission.allocated(0,sizeof(Context));
 if(!context->storage){status="Carte SD en cours de reconnexion";return false;}
 strcpy(context->argument,argument);
 if(language&&language[0]=='f'&&language[1]=='r'){context->language[0]='f';context->language[1]='r';}
 context->api={TAB5_APP_ABI_V1,sizeof(tab5_app_api),context->language,appLog,appStop,appSleep,appAllocate,appRelease,appOpen,appRead,appSeek,appClose,appMillis};
 context->api2={context->api,appSize,appVideoBegin,appPresent,appButtons,appAudio,appSave};context->api2.base.version=TAB5_APP_ABI_V2;context->api2.base.size=sizeof(tab5_app_api2);
 context->api3={context->api2,appHardware};context->api3.base.base.version=TAB5_APP_ABI_V3;context->api3.base.base.size=sizeof(tab5_app_api3);
 esp_elf_init(&context->elf);
 // Registration is idempotent; the sole table has static lifetime.
 esp_elf_register_symbol(symbols);
 int result=esp_elf_relocate(&context->elf,payload);
 if(result){status=result==-ENOMEM?"Mémoire insuffisante pour l'application":"Chargement de l’application impossible";return false;}
 admission.allocated(0,imageBytes);
 const int syncResult=tab5_elf_sync(&context->elf,image.bytes);
 if(syncResult!=0){Tab5DebugSerial.printf("[ELF] cache sync error=%d addr=%p bytes=%lu\n",syncResult,context->elf.psegment,(unsigned long)image.bytes);status="Préparation de l’application impossible";return false;}
 // Release the source file buffer before creating the worker, but retain image.
 heap_caps_free(payload);payloadFree.p=nullptr;
 if(cancelled.load(std::memory_order_acquire))return false;
 // The normal ELF uses this loader's already-admitted internal stack. Only
 // applications declaring a larger stack need a separate PSRAM-stack task.
 if(context->externalStack){
  const auto started=xTaskCreatePinnedToCoreWithCaps(task,"tab5_elf",image.stack,context,2,&context->worker,0,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  admission.release();
  if(started!=pdPASS){status="Mémoire insuffisante pour démarrer l’application";return false;}
 }
 return true;
}
void launchTask(void* argument){
 auto* request=static_cast<LaunchRequest*>(argument);
 const bool loaded=loadApplication(*request);
 heap_caps_free(request);
 if(!loaded){
  const bool stopped=cancelled.load(std::memory_order_acquire);
  if(stopped)status="Application arrêtée";
  loadFailed.store(!stopped,std::memory_order_relaxed);
  tab5TaskCheckpoint("chargement ELF");
  // No Context access after this publication. The UI alone cleans even a
  // partially relocated image, after observing launching=false.
  released.store(true,std::memory_order_release);
  launching.store(false,std::memory_order_release);
  return;
 }
 auto* c=context;
 xSemaphoreTake(consoleLock(),portMAX_DELAY);consoleLength=0;if(console)console[0]=0;revision.fetch_add(1,std::memory_order_release);xSemaphoreGive(consoleLock());
 status="Application en cours";running.store(true,std::memory_order_release);
 if(c->externalStack){
  tab5TaskCheckpoint("chargement ELF");
  launching.store(false,std::memory_order_release);
  // This is the final Context access by the loader. The external worker can
  // start/finish immediately; its existing suspend/delete barrier owns exit.
  c->startReady.store(true,std::memory_order_release);
  return;
 }
 launching.store(false,std::memory_order_release);
 task(c); // Same 16 KiB stack, priority and core as the previous app worker.
}
}
bool tab5ExecutableRun(const char* path,const char* argument,const char* language){
 tab5ExecutableService();
 if(launching.load(std::memory_order_acquire)||context){status="Une application est déjà en cours";return false;}
 char resolvedPath[261];if(!tab5LaunchPath(path,resolvedPath,sizeof(resolvedPath))){status="Chemin invalide";return false;}path=resolvedPath;
 if(!argument)argument="";
 if(!validPath(path)){status="Chemin invalide";return false;}
 size_t length=strlen(path);if(length<4||strcasecmp(path+length-4,".elf")){status="Choisissez une application Tab5 .elf";return false;}
 if(strnlen(argument,MaxArgument+1)>MaxArgument){status="Argument trop long";return false;}
 registerCache();
 auto budget=Tab5Resources::TaskBudget(StackBytes);budget.psram=sizeof(LaunchRequest);budget.psramBlock=sizeof(LaunchRequest);
 auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,budget,"chargement ELF");
 if(!admission){status="Mémoire insuffisante pour démarrer l’application";return false;}
 auto* request=static_cast<LaunchRequest*>(heap_caps_malloc(sizeof(LaunchRequest),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!request){status="Mémoire insuffisante pour l'application";return false;}
 admission.allocated(0,sizeof(LaunchRequest));
 strcpy(request->path,path);strcpy(request->argument,argument);strcpy(request->language,language&&language[0]=='f'&&language[1]=='r'?"fr":"en");
 strcpy(sourcePath,path);cancelled=false;released=false;loadFailed=false;launchErrorPending=false;
 memoryCheckPending=false;tab5LogMemory("ELF avant lancement");
 status="Chargement de l'application";launching.store(true,std::memory_order_release);
 if(tab5CreateAdmittedTask(admission,launchTask,"tab5_elf",StackBytes,request,2,nullptr,0)!=pdPASS){
  heap_caps_free(request);launching.store(false,std::memory_order_release);status="Mémoire insuffisante pour démarrer l’application";
  tab5LogMemory("ELF lancement abandonne");memoryCheckAt=millis();memoryCheckPending=true;return false;
 }
 return true; // Accepted; asynchronous load errors are reported by Service.
}
void tab5ExecutableStop(){if(launching.load(std::memory_order_acquire)||context){cancelled.store(true,std::memory_order_release);status="Arrêt demandé — attente de coopération";}}
void tab5ExecutableService(){
 // The loader may still construct Context or request cache collection. Do not
 // inspect its pointers or call LVGL until its release publication is visible.
 if(launching.load(std::memory_order_acquire))return;
 Tab5NativeView::service();
 // A self-deleting task's stack/TCB belongs to the FreeRTOS idle cleanup.
 // Sample again later instead of reporting that delayed storage as a leak.
 if(memoryCheckPending&&!context&&!launching&&uint32_t(millis()-memoryCheckAt)>=250U){memoryCheckPending=false;tab5LogMemory("ELF controle differe (250 ms)");}
 if(context&&context->externalStack&&context->worker&&!context->startReady.load(std::memory_order_acquire)&&eTaskGetState(context->worker)==eSuspended){vTaskDeleteWithCaps(context->worker);context->worker=nullptr;released.store(true,std::memory_order_release);}
 if(!released.exchange(false,std::memory_order_acquire))return;
 if(loadFailed.exchange(false,std::memory_order_acquire))launchErrorPending=true;
 else {const bool failed=!cancelled.load()&&context&&context->result;status=cancelled.load()?"Application arrêtée":context&&context->result?"Erreur de l'application — voir la console":"Application terminée";if(failed)launchErrorPending=true;}
 cleanup();running.store(false,std::memory_order_release);
 tab5LogMemory("ELF ressources liberees");memoryCheckAt=millis();memoryCheckPending=true;
}
bool tab5ExecutableRunning(){return running.load(std::memory_order_acquire);}
bool tab5ExecutableBusy(){return launching.load(std::memory_order_acquire)||context!=nullptr;}
bool tab5ExecutableTakeLaunchError(){const bool pending=launchErrorPending;launchErrorPending=false;return pending;}
const char* tab5ExecutableStatus(){return status.load(std::memory_order_acquire);}
const char* tab5ExecutableSourcePath(){return sourcePath;}
uint32_t tab5ExecutableRevision(){return revision.load(std::memory_order_acquire);}
size_t tab5ExecutableCopyOutput(char* destination,size_t capacity){
 if(!destination||!capacity)return 0;
 if(launching.load(std::memory_order_acquire)){destination[0]=0;return 0;}
 xSemaphoreTake(consoleLock(),portMAX_DELAY);const char* first=console?console:"";while(*first&&(uint8_t(*first)&0xc0)==0x80)++first;
 size_t length=std::min(strlen(first),capacity-1);memcpy(destination,first,length);destination[length]=0;xSemaphoreGive(consoleLock());return length;
}
const char* tab5ExecutableConsoleSnapshot(){
 if(launching.load(std::memory_order_acquire))return "";
 registerCache();if(!snapshot){auto budget=Tab5Resources::Budget{};budget.psram=cacheMissing();budget.psramBlock=ConsoleCapacity;
  auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,budget,"console ELF");
  // A collection can release a console which existed at the initial sample.
  if(admission&&budget.psram<cacheMissing()){admission.release();budget.psram=cacheMissing();admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,budget,"console ELF");}
  if(!admission||!createCache(admission))return "";
 }
 uint32_t current=revision.load(std::memory_order_acquire);if(snapshotRevision==current)return snapshot;
 xSemaphoreTake(consoleLock(),portMAX_DELAY);const char* first=console?console:"";while(*first&&(uint8_t(*first)&0xc0)==0x80)++first;
 size_t length=strlen(first);memcpy(snapshot,first,length+1);snapshotRevision=revision.load(std::memory_order_relaxed);xSemaphoreGive(consoleLock());return snapshot;
}
void tab5ExecutableClearOutput(){if(launching.load(std::memory_order_acquire))return;xSemaphoreTake(consoleLock(),portMAX_DELAY);consoleLength=0;if(console)console[0]=0;revision.fetch_add(1,std::memory_order_release);xSemaphoreGive(consoleLock());}
