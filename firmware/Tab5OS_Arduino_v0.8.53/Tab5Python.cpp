#include "Tab5Serial.h"
#include "Tab5Environment.h"
#include "Tab5Hardware.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5FilePolicy.h"
#include "Tab5Python.h"
#include "Tab5PythonArguments.h"
#include "Tab5SystemPath.h"
#include "WriterGamepadNavigation.h"
#include "Tab5Tools.h"
#include "Tab5WikiConvert.h"
#include "Tab5ResourceManager.h"
#include "Tab5Cooperate.h"
#include "Tab5Memory.h"
#include "Tab5PythonUI.h"
#include "Tab5PythonServices.h"
#include "Tab5TileValidation.h"
#include "Tab5TilePolicy.h"
#include <sys/stat.h>
#include "Tab5Localization.h"
#include "Tab5Diagnostics.h"
#include "Tab5Task.h"
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <esp32-hal-i2c.h>
#include <driver/uart.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>
#include <ctime>
#include <cerrno>
#include <cstring>
extern "C" int tab5_mp_run_with_argument(const char*,const char*,const char*,void*,size_t);
extern "C" int tab5_mp_open_files(void);
namespace {
std::atomic<bool> active{false},cancelled{false},error{false};
std::atomic<const char*> lastStatus{"Prêt"};
char admissionStatus[512]{}; // UI writes before starting the worker; stable lifetime.
constexpr size_t ConsoleCapacity=8192;
char* console=nullptr;size_t consoleLength=0;
std::atomic<uint32_t> consoleRevision{0};
std::atomic<uint32_t> vmStarts{0};
StaticSemaphore_t consoleMutexStorage;
SemaphoreHandle_t consoleMutex(){static auto m=xSemaphoreCreateMutexStatic(&consoleMutexStorage);return m;}
void* vmHeap=nullptr;
char* source=nullptr;char sourcePath[264]{},sourceArgument[261]{};
Tab5StorageUse* workerStorage=nullptr;
void runPython(){
 tab5DiagnosticMark(1,"demarrage",sourcePath,1);
 Tab5StorageUse storage;workerStorage=&storage;
 Tab5DebugSerial.printf("[PY] debut %s\n",sourcePath);tab5LogMemory("Python: avant VM");
 void* heap=vmHeap;
 if(heap&&storage){vmStarts.fetch_add(1,std::memory_order_relaxed);error=tab5_mp_run_with_argument(source,sourcePath,sourceArgument,heap,2*1024*1024)!=0;}
 else {error=true;const char* m="MemoryError: MicroPython heap or SD temporarily unavailable\n";xSemaphoreTake(consoleMutex(),portMAX_DELAY);strcpy(console,m);consoleLength=strlen(m);++consoleRevision;xSemaphoreGive(consoleMutex());}
 Tab5DebugSerial.printf("[PY] fin, erreur=%d, annule=%d\n",int(error.load()),int(cancelled.load()));tab5LogMemory("Python: fin VM");
 Tab5PythonUI::releaseStorage();Tab5PythonServices::reset();Tab5Hardware::release(Tab5Hardware::Owner::Python);heap_caps_free(heap);vmHeap=nullptr;heap_caps_free(source);source=nullptr;workerStorage=nullptr;}
void task(void*){
 runPython();
 lastStatus=cancelled.load()?"Script arrêté":error.load()?"Erreur Python — voir la console":"Script terminé";
 tab5DiagnosticMark(1,"termine",sourcePath,0);
 active.store(false,std::memory_order_release);vTaskDelete(nullptr);
}
}
extern "C" int tab5_mp_cancelled(){return cancelled.load();}
extern "C" void tab5_mp_phase(const char* phase,const char* filename){
 tab5DiagnosticMark(1,phase,filename?filename:sourcePath,1);
 if(!strcmp(phase,"ouvrir")||!strcmp(phase,"GC"))return;
 // Coarse VM boundaries only. These diagnostics identify an import/parser/GC
 // failure separately from a native I/O error, with the REAL task stack low.
 Tab5DebugSerial.printf("[PY] %s %s | pile libre mini=%u o, RAM libre=%u o, bloc=%u o\n",
  phase,filename?filename:"",unsigned(uxTaskGetStackHighWaterMark(nullptr)),
  unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
  unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
}
extern "C" void tab5_mp_yield(){static Tab5TimeSlice slice;if(slice.due(millis(),2))vTaskDelay(1);}
extern "C" void tab5_mp_output(const char* str,size_t n){
 if(!console)return;
 if(n>ConsoleCapacity-1){str+=n-(ConsoleCapacity-1);n=ConsoleCapacity-1;}
 xSemaphoreTake(consoleMutex(),portMAX_DELAY);
 if(consoleLength+n>=ConsoleCapacity){size_t drop=consoleLength+n-ConsoleCapacity+1;memmove(console,console+drop,consoleLength-drop);consoleLength-=drop;}
 memcpy(console+consoleLength,str,n);consoleLength+=n;console[consoleLength]=0;
 ++consoleRevision;
 xSemaphoreGive(consoleMutex());
}
// Native bridge uses bounded transfers and leaves all OS-owned buses/pins alone.
extern "C" int tab5_mp_io(int op,int a,int b,int c,const void* input,size_t len,void* output,size_t capacity){
 if(len>4096)return -EINVAL;
 if(op>=107&&op<=109)return tab5_env_call(op,a,input,len,output,capacity);
 if(op==106)return writing::gamepadPythonCall(a,b,output,capacity);
 if((op>=2&&op<=7)||(op>=9&&op<=40))return Tab5Hardware::call(Tab5Hardware::Owner::Python,op,a,b,c,input,len,output,capacity);
 if(op==98){
  if(!workerStorage||tab5_mp_open_files()||!Tab5PythonServices::storageIdle())return -EBUSY;
  const unsigned completion=tab5StorageRecoveryCompletions();
  workerStorage->release();tab5StorageRequestRecoveryFromWorker();
  const uint32_t started=millis();
  while(tab5StorageRecoveryCompletions()==completion&&millis()-started<12000&&!cancelled.load())vTaskDelay(5);
  // Do not let the VM resume until it owns its lease again, including cancellation.
  while(!workerStorage->acquire())vTaskDelay(1);
  if(cancelled.load())return -ECANCELED;
  if(tab5StorageRecoveryCompletions()==completion)return -EBUSY;
  return tab5StorageLastError()?-tab5StorageLastError():0;
 }
 if(op==81){
  // Python sends a NUL-terminated UTF-8 source. Copy under the locale mutex:
  // no SD read, duplicate dictionary or dangling pointer after a language swap.
  if(!input||!len||!capacity||!output||((const char*)input)[len-1]||memchr(input,0,len-1))return -EINVAL;
  size_t n=Tab5Locale.copyText((const char*)input,(char*)output,capacity);
  return n>=capacity?-ENOSPC:int(n);
 }
 if(op==97){
  // Decode outside the 2 MiB VM heap; immutable pack entries use bounded slices.
  if(!input||!len||len>260||b<0||size_t(b)>tab5tile::MaxBytes)return -EINVAL;
  const auto* p=static_cast<const char*>(input);if(p[0]!='/'||memchr(p,0,len))return -EINVAL;
  size_t start=1;
  for(size_t i=0;i<=len;++i){
   if(i<len&&(uint8_t(p[i])<32||p[i]=='\\'||p[i]==':'))return -EINVAL;
   if(i==len||p[i]=='/'){
    const size_t count=i>=start?i-start:0;
    if((count==1&&p[start]=='.')||(count==2&&p[start]=='.'&&p[start+1]=='.'))return -EINVAL;
    start=i+1;
   }
  }
  char name[264];memcpy(name,"/sd",3);if(!tab5SystemPath(name+3,sizeof(name)-3,p,len))return -ENAMETOOLONG;
  struct stat info{};if(stat(name,&info)!=0)return -(errno?errno:EIO);
  File file=tab5FS.open(name+3,FILE_READ);if(!file||file.isDirectory())return -EIO;
  const uint32_t offset=uint32_t(a);const uint64_t total=file.size();
  const uint64_t length=b?uint32_t(b):(total>=offset?total-offset:0);
  if(offset>total||length>total-offset){file.close();return 0;}
  if(length>tab5tile::MaxBytes){file.close();return -EFBIG;}
  if(!file.seek(offset)){file.close();return -EIO;}
  Tab5TileValidation::Source source{&file,size_t(length),
    [](void* f,uint8_t* out,size_t count){return static_cast<File*>(f)->read(out,count);}};
  const auto result=Tab5TileValidation::validateSource(source,[](){return cancelled.load();});file.close();
  return result==Tab5TileValidation::Result::Valid?1:
    result==Tab5TileValidation::Result::Invalid?0:
    result==Tab5TileValidation::Result::NoMemory?-ENOMEM:-EIO;
 }
 if(op==105||(op>=50&&op<=59)||op==90||op==91||op==92||op==94||op==95||op==96)return Tab5PythonUI::call(op,a,b,c,input,len,output,capacity);
 if((op>=60&&op<=69)||op==80||op==104||(op>=82&&op<=89))return Tab5PythonServices::call(op,a,b,c,input,len,output,capacity);
 switch(op){
 case 102:tab5RequestNativeTheme();return 0;
 case 103:return tab5WikiConvert(input,len);
 case 101:return -ENOSYS;
 case 99:return WiFi.status()==WL_CONNECTED?1:0;
 case 0:return int(millis()&0x3fffffff);
 case 1:if(a<0||a>3600000)return -EINVAL;for(int t=0;t<a&&!cancelled;t+=10)delay(std::min(10,a-t));return 0;
 case 8:{if(!output)return -EINVAL;if(capacity<8)return -ENOSPC;const time_t now=time(nullptr);const uint64_t seconds=now>=1577836800?uint64_t(now):0;auto* bytes=static_cast<uint8_t*>(output);for(unsigned i=0;i<8;++i)bytes[i]=uint8_t(seconds>>(8*i));return 8;}
 default:return -ENOSYS;
 }
}
namespace Tab5Python {
bool runFile(const char* path,const char* argument){
 if(running()){lastStatus="Un script est déjà en cours";return false;}
 std::string canonical;
 if(!path||!Tab5Files::canonical(path,canonical)){lastStatus="Chemin invalide";return false;}
 String lower=path;lower.toLowerCase();
 if(!lower.endsWith(".py")){lastStatus="Exécution disponible pour les fichiers Python .py";return false;}
 Tab5StorageUse storage;if(!storage){lastStatus="Carte SD en cours de reconnexion";return false;}
 File file=tab5FS.open(path);
 if(!file||file.isDirectory()||file.size()>65536){lastStatus="Script inaccessible ou supérieur à 64 Kio";return false;}
 size_t size=file.size();auto* code=static_cast<char*>(heap_caps_malloc(size+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!code){lastStatus="PSRAM insuffisante pour le script";return false;}
 size_t got=file.read(reinterpret_cast<uint8_t*>(code),size);file.close();code[got]=0;
 if(got!=size||memchr(code,0,size)){
  heap_caps_free(code);lastStatus="Lecture du fichier impossible";
  if(got!=size)tab5StorageFault("Lecture script Python",path,EIO);return false;
 }
 bool started=run(code,path,argument);heap_caps_free(code);return started;
}
bool run(const char* code,const char* path,const char* argument){
 if(active){lastStatus="Un script est déjà en cours";return false;}
 if(!code||!path||strlen(code)>65536||strlen(path)>260){lastStatus="Script ou chemin invalide";return false;}
 char checkedArgument[261];
 if(!Tab5PythonArguments::copy(argument,checkedArgument,sizeof(checkedArgument))){lastStatus="Chemin invalide";return false;}
 if(*checkedArgument&&!tab5FS.exists(checkedArgument)){lastStatus="Fichier inaccessible";return false;}
 Tab5Resources::AdmissionReport report;
 auto admission=tab5ReserveMemory(Tab5Resources::Owner::Python,Tab5Resources::PythonBudget,"MicroPython",&report);
 if(!admission){Tab5Resources::describeAdmission(report,admissionStatus,sizeof(admissionStatus));lastStatus=admissionStatus;return false;}
 if(!console)console=static_cast<char*>(heap_caps_calloc(ConsoleCapacity,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!console){lastStatus="PSRAM insuffisante pour la console";return false;}
 source=static_cast<char*>(heap_caps_malloc(strlen(code)+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!source){lastStatus="PSRAM insuffisante pour le script";return false;}
 vmHeap=heap_caps_malloc(2*1024*1024,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!vmHeap){lastStatus="PSRAM insuffisante pour MicroPython";heap_caps_free(source);source=nullptr;return false;}
 admission.allocated(0,2*1024*1024+strlen(code)+1);
 writing::gamepadPythonReset();
 Tab5PythonUI::reset();
 strcpy(source,code);strcpy(sourcePath,path);strcpy(sourceArgument,checkedArgument);cancelled=false;error=false;
 xSemaphoreTake(consoleMutex(),portMAX_DELAY);consoleLength=0;console[0]=0;++consoleRevision;xSemaphoreGive(consoleMutex());
 lastStatus="Script en cours";
 active=true;if(tab5CreateAdmittedTask(admission,task,"tab5_python",32768,nullptr,1,nullptr,0)!=pdPASS){lastStatus="RAM interne insuffisante pour la tâche Python";active=false;heap_caps_free(vmHeap);vmHeap=nullptr;heap_caps_free(source);source=nullptr;return false;}return true;
}
void stop(){cancelled=true;}
bool running(){return active.load(std::memory_order_acquire);}
uint32_t vmStartCount(){return vmStarts.load(std::memory_order_relaxed);}
const char* runningPath(){return running()?sourcePath:"";}
const char* runningArgument(){return running()?sourceArgument:"";}
bool failed(){return error.load()&&!cancelled.load();}
const char* status(){return lastStatus.load();}
uint32_t outputRevision(){return consoleRevision.load(std::memory_order_acquire);}
void clearOutput(){xSemaphoreTake(consoleMutex(),portMAX_DELAY);consoleLength=0;if(console)console[0]=0;++consoleRevision;xSemaphoreGive(consoleMutex());}
size_t copyOutput(char* destination,size_t capacity){
 if(!destination||!capacity)return 0;
 xSemaphoreTake(consoleMutex(),portMAX_DELAY);
 const char* first=console?console:"";while(*first&&(uint8_t(*first)&0xc0)==0x80)++first;
 size_t n=std::min(strlen(first),capacity-1);
 // Do not publish half a UTF-8 character when the caller uses a small buffer.
 if(first[n])while(n&&(uint8_t(first[n])&0xc0)==0x80)--n;
 memcpy(destination,first,n);destination[n]=0;
 xSemaphoreGive(consoleMutex());return n;
}

String output(){
 String result;
 // Only the immutable copy leaves this mutex. No interrupt-disabled memcpy
 // of 8 KiB, and no second permanently reserved internal-RAM array.
 xSemaphoreTake(consoleMutex(),portMAX_DELAY);
 if(console){const char* p=console;while(*p&&(uint8_t(*p)&0xc0)==0x80)++p;result=p;}
 xSemaphoreGive(consoleMutex());
 return result;
}
}
