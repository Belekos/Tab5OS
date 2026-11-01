#include "Tab5AppPanel.h"
#include "Tab5ConsoleBuffer.h"
#include "Tab5NetworkTools.h"
#include "Tab5Localization.h"
#include "Tab5ResourceManager.h"
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <new>
namespace Tab5AppPanel {
namespace {
struct Context {
 Specification spec;
 tab5_app_panel_event request{};
 char output[Tab5ConsoleBuffer::Capacity]{},snapshot[Tab5ConsoleBuffer::Capacity]{};
 size_t length=0;
 bool truncated=false,pendingRun=false,pendingClear=false,closed=false,cancelled=false;
 uint32_t state=TAB5_PANEL_READY;
};
Context* context=nullptr;
std::atomic<bool> requested{false};
std::atomic<uint32_t> changes{0};
StaticSemaphore_t mutexStorage;
SemaphoreHandle_t mutex(){static auto m=xSemaphoreCreateMutexStatic(&mutexStorage);return m;}
struct Lock {Lock(){xSemaphoreTake(mutex(),portMAX_DELAY);}~Lock(){xSemaphoreGive(mutex());}};
template<size_t N> bool copy(char (&to)[N],const char* from){
 if(!from){to[0]=0;return true;}size_t n=0;while(n<N&&from[n])++n;
 if(n==N)return false;memcpy(to,from,n+1);return true;
}
bool fill(Specification& out,const tab5_app_panel_spec& in){
 if(!copy(out.title,in.title)||!copy(out.modes,in.modes)||!copy(out.targetLabel,in.target_label)||
    !copy(out.targetHint,in.target_hint)||!copy(out.valueLabel,in.value_label)||
    !copy(out.value,in.value)||!copy(out.runLabel,in.run_label)||!copy(out.initialTarget,in.initial_target)||!copy(out.alternateHint,in.alternate_hint))return false;
 if(!*out.title||!*out.modes||!*out.targetLabel||!*out.runLabel)return false;
 out.modeCount=1;for(const char* p=out.modes;*p;++p)if(*p=='\n'){if(p==out.modes||p[-1]=='\n'||!p[1])return false;++out.modeCount;}
 if(out.modeCount>8||((in.value_modes|in.alternate_modes)&~((1u<<out.modeCount)-1)))return false;
 out.valueModes=in.value_modes;out.alternateModes=in.alternate_modes;return true;
}
void clearLocked(){context->length=0;context->truncated=false;context->output[0]=0;++changes;}
}
int open(const tab5_app_panel_spec* spec){
 if(!spec||spec->size!=sizeof(*spec))return -EINVAL;
 {Lock lock;if(context)return -EBUSY;}
 // Only the owning ELF worker can open; UI cannot create a concurrent context.
 auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,{0,0,0,sizeof(Context),sizeof(Context)},"console ELF");
 if(!admission)return -ENOMEM;
 void* memory=heap_caps_malloc(sizeof(Context),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!memory)return -ENOMEM;
 auto* fresh=new(memory)Context;
 if(!fill(fresh->spec,*spec)){fresh->~Context();heap_caps_free(fresh);return -EINVAL;}
 admission.allocated(0,sizeof(Context));
 {Lock lock;context=fresh;++changes;}
 requested.store(true,std::memory_order_release);return 0;
}
int poll(tab5_app_panel_event* event){
 if(!event||event->size!=sizeof(*event))return -EINVAL;
 Lock lock;if(!context)return -1;
 if(context->closed){event->action=TAB5_PANEL_CLOSE;return 1;}
 if(context->pendingClear){context->pendingClear=false;event->action=TAB5_PANEL_CLEAR;return 1;}
 if(!context->pendingRun){event->action=0;return 0;}
 *event=context->request;context->pendingRun=false;return 1;
}
int write(const char* data,size_t length,int reset){
 if(length>4096||(!data&&length))return -EINVAL;
 Lock lock;if(!context||context->closed)return -1;
 if(reset)clearLocked();
 if(Tab5ConsoleBuffer::append(context->output,sizeof(context->output),context->length,context->truncated,data,length))++changes;
 return 0;
}
int state(uint32_t value){
 if(value>TAB5_PANEL_STOPPED)return -EINVAL;
 Lock lock;if(!context||context->closed)return -1;
 context->state=value;++changes;return 0;
}
bool takeOpenRequest(){return requested.exchange(false,std::memory_order_acquire);}
bool active(){Lock lock;return context&&!context->closed;}
bool closing(){Lock lock;return context&&context->closed;}
bool busy(){Lock lock;return context&&(context->pendingRun||context->state==TAB5_PANEL_RUNNING);}
bool cancelled(){Lock lock;return context&&(context->closed||context->cancelled);}
const Specification* specification(){Lock lock;return context?&context->spec:nullptr;}
bool submit(uint32_t mode,const char* target,const char* value){
 Lock lock;if(!context||context->closed||context->pendingRun||context->state==TAB5_PANEL_RUNNING||mode>=context->spec.modeCount||!target||!*target)return false;
 if(!copy(context->request.target,target)||!copy(context->request.value,value))return false;
 context->request.size=sizeof(context->request);context->request.action=TAB5_PANEL_RUN;context->request.mode=mode;
 context->cancelled=false;context->pendingRun=true;context->state=TAB5_PANEL_RUNNING;++changes;return true;
}
void stop(){Lock lock;if(context)context->cancelled=true;}
void clear(){Lock lock;if(context){clearLocked();context->pendingClear=true;}}
void close(){Lock lock;if(context){context->closed=true;context->cancelled=true;context->pendingRun=false;}requested=false;}
void detach(){
 // The loader calls this on the UI task after the worker has stopped touching
 // both ELF memory and broker. No task delete, callback or borrowed pointer.
 Lock lock;requested=false;if(context){context->~Context();heap_caps_free(context);context=nullptr;++changes;}
}
uint32_t revision(){return changes.load();}
const char* snapshot(uint32_t& rev){Lock lock;rev=changes.load();if(!context)return "";memcpy(context->snapshot,context->output,context->length+1);return context->snapshot;}
const char* completedReport(size_t& length){Lock lock;length=0;if(!context||context->closed||context->pendingRun||context->state==TAB5_PANEL_RUNNING)return nullptr;length=context->length;return context->output;}
const char* status(){
 Lock lock;if(!context)return "Prêt";if(context->state==TAB5_PANEL_RUNNING)return context->cancelled?"Arrêt en cours.":"Analyse en cours.";
 if(context->truncated)return "Rapport tronqué";
 switch(context->state){case TAB5_PANEL_DONE:return "Analyse terminée";case TAB5_PANEL_ERROR:return "Analyse terminée avec des erreurs";case TAB5_PANEL_STOPPED:return "Analyse interrompue";default:return "Prêt";}
}
size_t translate(const char* key,char* output,size_t capacity){
 if(!key)return 0;size_t n=0;while(n<2049&&key[n])++n;
 if(n>2048||capacity>8192||(!output&&capacity))return 0;
 return Tab5Locale.copyText(key,output,capacity);
}
int network(int argc,const char* const* argv,void (*output)(void*,const char*,size_t),void* user,bool (*stop)()){
 if(argc<1||argc>8||!argv||!argv[0]||!output)return -EINVAL;
 size_t total=0;for(int i=0;i<argc;++i){if(!argv[i])return -EINVAL;size_t n=0;while(n<2049&&argv[i][n])++n;if(n>2048||(total+=n+1)>4096)return -EINVAL;}
 if(strcmp(argv[0],"dns")&&strcmp(argv[0],"ping")&&strcmp(argv[0],"ports")&&strcmp(argv[0],"headers"))return -EPERM;
 auto cancel=[&]{return (stop&&stop())||closing()||cancelled();};if(cancel())return 130;
 try {
 std::vector<std::string> args;args.reserve(size_t(argc));for(int i=0;i<argc;++i)args.emplace_back(argv[i]);
 auto emit=[&](const char* data,size_t length){while(length){size_t n=length>4096?4096:length;output(user,data,n);data+=n;length-=n;}};
 auto tr=[](const char* key){size_t n=Tab5Locale.copyText(key,nullptr,0);if(n>8191)return std::string(key);std::string result(n+1,'\0');size_t actual=Tab5Locale.copyText(key,&result[0],result.size());if(actual>n)return std::string(key);result.resize(actual);return result;};
 return Tab5NetworkTools::run(args,emit,cancel,tr);
 } catch(const std::bad_alloc&) {return -ENOMEM;}
 // No C++ exception may unwind through the freestanding C/ELF ABI.
 catch(...) {return -EIO;}
}
}
