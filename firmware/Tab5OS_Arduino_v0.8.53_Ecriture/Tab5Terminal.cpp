#include "Tab5Terminal.h"
#include "Tab5Serial.h"
#include "Tab5Environment.h"
#include "Tab5TerminalBuffer.h"
#include "Tab5Shell.h"
#include "Tab5Localization.h"
#include "Tab5Storage.h"
#include "Tab5VolumeFS.h"
#include "Tab5NetworkTools.h"
#include "Tab5Ssh.h"
#include "Tab5Git.h"
#include "Tab5Memory.h"
#include "Tab5Task.h"
#include "Tab5FilePolicy.h"
#include "Tab5TerminalPath.h"
#include "Tab5UsbStorage.h"
#include <esp_vfs_fat.h>
#include <WiFi.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <mbedtls/sha256.h>
#include <freertos/semphr.h>
#include <atomic>
#include <new>
#include <algorithm>
#include <cstring>
namespace Tab5Terminal {
namespace {
constexpr size_t Capacity=32768,StackBytes=16384;
std::atomic<bool> running{false},cancelled{false},released{true};std::atomic<uint32_t> change{0};bool closing=false;
StaticSemaphore_t mutexStorage;SemaphoreHandle_t mutex(){static auto m=xSemaphoreCreateMutexStatic(&mutexStorage);return m;}
struct Lock {Lock(){xSemaphoreTake(mutex(),portMAX_DELAY);}~Lock(){xSemaphoreGive(mutex());}};
struct Context {OutputRing<Capacity> output;char snapshot[Capacity]{};Tab5Shell::Engine* shell=nullptr;std::string command,cwd="/",draft;std::vector<Tab5Shell::Buffer> history;int position=0;bool sdReady=false;YieldCadence yield;};
Context* context=nullptr;
void append(const char* p,size_t n){Lock lock;if(!context||!n)return;context->output.append(p,n);++change;}
std::string tr(const char* source){size_t n=Tab5Locale.copyText(source,nullptr,0);if(n>8192)return std::string(source);std::string translated(n+1,'\0');size_t actual=Tab5Locale.copyText(source,&translated[0],translated.size());if(actual>n)return std::string(source);translated.resize(actual);return translated;}
void appendTranslated(const char* source){std::string text=tr(source);append(text.data(),text.size());}
int admin(const std::vector<std::string>& args,Tab5Shell::Engine::Output out){
 const int serialResult=Tab5Serial::command(args,out,[]{return cancelled.load();},tr,context->shell->input());if(serialResult!=-1)return serialResult;
 const auto& c=args[0];auto print=[&](const std::string& s){out(s.data(),s.size());};auto diagnostic=context->shell->diagnostics();auto fail=[&](const std::string& s){diagnostic(s.data(),s.size());};
 const int gitResult=Tab5Git::runIsolated("/sd",context->shell->cwd(),args,out,[]{return cancelled.load();},tr,diagnostic);if(gitResult!=-1)return gitResult;
 const int sshResult=Tab5Ssh::run(args,out,[]{return cancelled.load();},tr,context->shell->cwd(),diagnostic,context->shell->input());if(sshResult!=-1)return sshResult;
 if(c=="tasks"){if(args.size()!=1){fail("tasks\n");return 2;}return tab5DumpTasks([](const char* text,size_t size,void* value){(*static_cast<Tab5Shell::Engine::Output*>(value))(text,size);},&out)?0:1;}
 const int networkResult=Tab5NetworkTools::run(args,out,[]{return cancelled.load();},tr,context->shell->cwd(),diagnostic);if(networkResult!=-1)return networkResult;
 if(c=="memory"){print(tr("Internal free: ")+std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT))+tr(" bytes\n")+tr("Internal largest block: ")+std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT))+tr(" bytes\n")+tr("PSRAM free: ")+std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))+tr(" bytes\n")+tr("PSRAM largest block: ")+std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM))+tr(" bytes\n"));return 0;}
 if(c=="uptime"){print(std::to_string(esp_timer_get_time()/1000000)+tr(" seconds\n"));return 0;}
 if((c=="df"||c=="sha256")&&!context->sdReady){fail(tr("SD unavailable\n"));return 1;}
 if(c=="df"){
  std::string path;if(args.size()>2||!Tab5TerminalPath::logical(args.size()==2?args[1]:".",context->shell->cwd(),path)){fail(tr("usage: df [directory]\n"));return 2;}
  bool usb=Tab5UsbStorage::path(path.c_str());uint64_t total=0,free=0;
  if(esp_vfs_fat_info(usb?"/sd/USB":"/sd",&total,&free)!=ESP_OK){fail(tr("Impossible de lire l'espace disponible du support.")+"\n");return 1;}
  print(std::string(usb?"USB: ":"SD: ")+tr("bytes free: ")+std::to_string(free)+" / "+std::to_string(total)+"\n");return 0;
 }
 if(c=="wifi"){print(WiFi.status()==WL_CONNECTED?std::string(WiFi.SSID().c_str())+" "+WiFi.localIP().toString().c_str()+"\n":tr("disconnected\n"));return 0;}
 if(c=="sha256"){
  if(args.size()<2){fail(tr("usage: sha256 file...\n"));return 1;}
  for(size_t i=1;i<args.size();++i){std::string name=args[i],normal;if(!Tab5TerminalPath::logical(name,context->shell->cwd(),normal)){fail(tr("invalid path")+"\n");return 1;}FILE* f=fopen(("/sd"+normal).c_str(),"rb");if(!f){fail(tr("cannot read ")+name+"\n");return 1;}mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);bool ok=mbedtls_sha256_starts(&hash,0)==0;unsigned char buf[512],digest[32];size_t n;
   while(ok&&!cancelled.load()&&(n=fread(buf,1,sizeof(buf),f)))ok=mbedtls_sha256_update(&hash,buf,n)==0;if(ferror(f)||cancelled.load())ok=false;fclose(f);if(ok)ok=mbedtls_sha256_finish(&hash,digest)==0;mbedtls_sha256_free(&hash);if(!ok)return 1;char hex[65];for(unsigned j=0;j<32;++j)snprintf(hex+j*2,3,"%02x",digest[j]);print(std::string(hex)+"  "+name+"\n");
  }return 0;
 }return -1;
}
void worker(void*){
 {Tab5StorageUse storage;
  context->sdReady=bool(storage);context->shell->storageAvailable(bool(storage));
  
  int rc=context->shell->run(context->command);if(rc==130)appendTranslated("\nInterrupted\n");
  // Shell redirections use POSIX files; publish once after the command.
  if(storage)tab5FsChanged();
 }
 {Lock lock;context->cwd=context->shell->cwd();if(!context->command.empty()){volatile char* p=&context->command[0];for(size_t n=0;n<context->command.size();++n)p[n]=0;}std::string().swap(context->command);++change;}
 tab5TaskCheckpoint("terminal");running=false;
 // All shell, storage and lock destructors have run. Neither this worker nor
 // the trampoline accesses context after publication. The dynamic RTOS stack
 // may still be awaiting idle cleanup; SDK pthread keeps ownership of TLS 0.
 released.store(true,std::memory_order_release);
}
}
bool open(){service();if(context){closing=false;return true;}void* data=heap_caps_malloc(sizeof(Context),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!data)return false;context=new(data)Context;
 void* shell=heap_caps_malloc(sizeof(Tab5Shell::Engine),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!shell){context->~Context();heap_caps_free(context);context=nullptr;return false;}
 context->shell=new(shell)Tab5Shell::Engine("/sd",append,[]{if(cancelled.load())return true;if(context->yield.due(millis()))vTaskDelay(1);return cancelled.load();});context->shell->admin(admin);context->shell->translate(tr);
 context->shell->environment({[](const std::string& key,std::string& out){char value[513]{};int rc=tab5_env_get(key.c_str(),value,sizeof(value));if(rc>=0)out.assign(value,size_t(rc));volatile char* p=value;for(size_t n=0;n<sizeof(value);++n)p[n]=0;return rc;},[](const std::string& key,const std::string& value,bool secret){return secret?tab5_env_bind(key.c_str(),value.c_str()):tab5_env_set(key.c_str(),value.c_str());},[](const std::string& key){return tab5_env_unset(key.c_str());},[]{return Tab5Environment::names();}});closing=false;appendTranslated("Tab5 shell\nhelp / help command\n");return true;
}
bool submit(const char* command){if(!command||!*command||strlen(command)>4096||busy()||!open())return false;
 const bool hidden=Tab5Environment::privateCommand(command);context->command=command;if(!hidden&&(context->history.empty()||strcmp(context->history.back().data(),command))){if(context->history.size()==24)context->history.erase(context->history.begin());Tab5Shell::Buffer item(4096);if(item.append(command,strlen(command)))context->history.push_back(std::move(item));}context->position=context->history.size();context->draft.clear();
 std::string prompt=context->cwd+" $ "+(hidden?tr("Commande masquée"):std::string(command))+"\n";append(prompt.data(),prompt.size());cancelled=false;released=false;running=true;
 if(tab5CreateTask(worker,"tab5_shell",StackBytes,nullptr,1,nullptr,0)!=pdPASS){running=false;released=true;std::string().swap(context->command);appendTranslated("Cannot allocate terminal task\n");return false;}return true;
}
bool runScript(const char* path){if(!path||busy())return false;std::string normal;if(!Tab5TerminalPath::logical(path,"/",normal)||normal.size()<4)return false;std::string ext=normal.substr(normal.size()-3);std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(tolower(c));});if(ext!=".sh")return false;
 auto quote=[](const std::string& value){std::string text="'";for(char c:value){if(c=='\'')text+="'\\''";else text+=c;}return text+"'";};size_t slash=normal.rfind('/');std::string directory=slash?normal.substr(0,slash):"/";return submit(("cd "+quote(directory)+" && sh "+quote(normal)).c_str());}
bool busy(){return running.load()||!released.load(std::memory_order_acquire);}
void stop(){cancelled=true;}
void clearOutput(){Lock lock;if(context){context->output.clear();context->snapshot[0]=0;++change;}}
void close(){closing=true;stop();service();}
void service(){if(closing&&!busy()&&context){Lock lock;context->shell->~Engine();heap_caps_free(context->shell);
 context->shell=nullptr;context->~Context();heap_caps_free(context);context=nullptr;++change;}}
uint32_t revision(){return change.load();}
const char* snapshot(uint32_t& revision){Lock lock;revision=change.load();if(!context)return "";context->output.copy(context->snapshot,sizeof(context->snapshot));return context->snapshot;}
String output(){Lock lock;if(!context)return String();context->output.copy(context->snapshot,sizeof(context->snapshot));return String(context->snapshot);}
String directory(){Lock lock;return context?String(context->cwd.c_str()):String("/");}
String history(int direction,const char* current){if(!context||busy())return current;if(context->position==int(context->history.size()))context->draft=current;context->position=std::clamp(context->position+direction,0,int(context->history.size()));return context->position==int(context->history.size())?String(context->draft.c_str()):String(context->history[context->position].data());}
String complete(const char* current){if(!context||busy())return current;Tab5StorageUse lease;context->shell->storageAvailable(bool(lease));return context->shell->complete(current).c_str();}
}
