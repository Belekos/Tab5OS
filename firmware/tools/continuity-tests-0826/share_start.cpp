#include <atomic>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
using String=std::string;
static unsigned checks=0,allocations=0,releases=0,allocStep=0,failStep=0,taskCalls=0;
static bool pageFail=false,taskFail=false,memoryDenied=false;
static void check(bool c,const char* s){++checks;if(!c){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,WL_CONNECTED=3,WIFI_OFF=0,pdPASS=1;
void* heap_caps_malloc(size_t n,unsigned caps){check(caps==3,"share buffers in PSRAM");if(++allocStep==failStep)return nullptr;auto* p=std::malloc(n);if(p)++allocations;return p;}
void* heap_caps_calloc(size_t n,size_t m,unsigned caps){auto* p=heap_caps_malloc(n*m,caps);if(p)std::memset(p,0,n*m);return p;}
void heap_caps_free(void* p){if(p){++releases;std::free(p);}}
struct Wifi{int status(){return WL_CONNECTED;}int getMode(){return 1;}}WiFi;
bool tab5NetworkEnabled(){return true;}uint32_t esp_random(){return 123456;}uint32_t millis(){return 42;}
struct Context{char* page;size_t pageLength;char data[4096],token[33];bool ap;int socket;};
struct Locale{String effectiveId(){return "en-US";}size_t copyText(const char*,char*,size_t){return 0;}}Tab5Locale;
namespace Tab5WebLocale {template<class T>size_t page(char* out,size_t,const char*,char*,size_t,T){if(pageFail)return 0;std::strcpy(out,"page");return 4;}}
std::atomic<bool> busy{false},ready{false},enabled{false},direct{false};std::atomic<const char*> lastError{""};char shareUrl[128]{},shareSsid[64]{},sharePassword[64]{};std::atomic<unsigned>code{0},startedAt{0};
void worker(void*){}
int tab5CreateTask(void(*)(void*),const char*,unsigned stack,void*,unsigned,std::nullptr_t,int){check(stack==8192,"share task stack unchanged");++taskCalls;return taskFail||memoryDenied?0:pdPASS;}
/* SHARE_START */
int main(){
 for(unsigned cycle=0;cycle<2000;++cycle){
  for(unsigned mode=1;mode<=5;++mode){busy=false;allocStep=0;failStep=mode<=2?mode:0;pageFail=mode==3;taskFail=mode==4;memoryDenied=mode==5;
   check(!tab5FileShareStart(false,false),"failed startup reported");check(!busy&&!enabled&&!ready&&!code,"failed startup leaves no active session");check(allocations==releases,"startup failure frees all PSRAM allocations");
  }
 }
 check(taskCalls==4000,"creation admission reached on task/refusal scenarios");
 std::printf("PASS %u share startup checks; failure_cycles=10000; retained_allocations=0\n",checks);return 0;
}
