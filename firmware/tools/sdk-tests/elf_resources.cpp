#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <malloc.h>
#include "Tab5ResourceManager.h"
#include "Tab5AppApi.h"
static unsigned checks=0,cycles=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(2);}}while(0)
static std::map<void*,size_t> blocks;
static size_t live=0,peak=0;static int failAfter=-1;
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
void* heap_caps_aligned_alloc(size_t alignment,size_t bytes,unsigned){
 if(failAfter==0){++failures;return nullptr;}if(failAfter>0)--failAfter;
 void* p=_aligned_malloc(bytes,alignment);CHECK(p);blocks[p]=bytes;live+=bytes;peak=std::max(peak,live);return p;
}
void* heap_caps_calloc(size_t count,size_t bytes,unsigned caps){void* p=heap_caps_aligned_alloc(16,count*bytes,caps);if(p)std::memset(p,0,count*bytes);return p;}
void heap_caps_free(void* p){if(!p)return;auto i=blocks.find(p);CHECK(i!=blocks.end());live-=i->second;blocks.erase(i);_aligned_free(p);}
Tab5Resources::MemorySample sample(){size_t free=32u*1024u*1024u-live;return {256u*1024u,128u*1024u,128u*1024u,free,free};}
Tab5Resources::MemoryTotals totals(){auto s=sample();return {s.internal,s.psram};}
Tab5Resources::Manager Tab5MemoryManager(sample,totals);
Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner who,const Tab5Resources::Budget& b,const char*,Tab5Resources::AdmissionReport* report){return Tab5MemoryManager.reserve(who,b,report);}
static unsigned files=0,storage=0,hardware=0,panelAttachments=0;
struct File{bool open=false;void close(){if(open){CHECK(files>0);--files;open=false;}}~File(){close();}};
struct Tab5StorageUse{Tab5StorageUse(){++storage;}~Tab5StorageUse(){CHECK(storage>0);--storage;}};
struct esp_elf_t{void* psegment=nullptr;};
void esp_elf_free(void* p){heap_caps_free(p);}
#include "elf-deinit-production.inc"
struct Allocation{void* pointer=nullptr;size_t bytes=0;};
struct Context{Tab5StorageUse storage;esp_elf_t elf;File files[TAB5_APP_MAX_FILES];Allocation allocations[32];size_t allocated=0;unsigned abi=1;bool extended=false;};
Context* context=nullptr;
bool worker(){return context!=nullptr;}bool cooperate(){return worker();}
namespace Tab5Hardware{enum class Owner{Application};void release(Owner){hardware=0;}}
namespace Tab5AppPanel{void detach(){panelAttachments=0;}}
static unsigned cacheDrops=0;static void* imageCache=nullptr;
namespace Tab5NativeView {
std::atomic<bool> ready{false},configuring{false};std::atomic<uint32_t> touchButtons{0};
uint16_t* front=nullptr;uint16_t* back=nullptr;unsigned width=0,height=0;bool extendedButtons=false;
void *screen=nullptr,*picture=nullptr,*previous=nullptr,*fpsLabel=nullptr,*volumeSlider=nullptr,*volumeLabel=nullptr,*controls=nullptr,*padLabel=nullptr,*padConfigure=nullptr;
int padStep=-1;struct Image{void* data=nullptr;}image;
void* lv_screen_active(){return nullptr;}bool lv_obj_is_valid(void*){return false;}void lv_screen_load(void*){CHECK(false);}
struct Focus{void clear(){}}Tab5Focus;
void lv_obj_delete(void*){CHECK(false);}
void lv_image_cache_drop(const void*){++cacheDrops;heap_caps_free(imageCache);imageCache=nullptr;}
#include "native-close-production.inc"
}
constexpr size_t ConsoleCapacity=8192;
char *console=nullptr,*snapshot=nullptr;size_t consoleLength=0;uint32_t snapshotRevision=UINT32_MAX;
std::atomic<uint32_t> revision{0};std::atomic<bool> running{false};bool launching=false;
constexpr int portMAX_DELAY=-1;int consoleLock(){return 1;}void xSemaphoreTake(int,int){}void xSemaphoreGive(int){}
#include "elf-allocation-production.inc"
#include "elf-release-production.inc"
#include "elf-cleanup-production.inc"
#include "elf-cache-create-production.inc"
#include "elf-cache-reclaim-production.inc"
void start(unsigned abi){
 CHECK(!context&&live==0&&files==0&&storage==0);
 context=new(heap_caps_aligned_alloc(16,sizeof(Context),0)) Context;context->abi=abi;context->extended=abi>=2;hardware=3;panelAttachments=1;
}
void checkEmpty(){CHECK(!context&&live==0&&blocks.empty()&&!files&&!storage&&!hardware&&!imageCache&&!panelAttachments);CHECK(!Tab5NativeView::front&&!Tab5NativeView::back&&!Tab5NativeView::image.data&&!Tab5NativeView::ready.load()&&!Tab5NativeView::width&&!Tab5NativeView::height&&!Tab5NativeView::extendedButtons&&!Tab5NativeView::touchButtons.load());}
int main(){
 for(unsigned repeat=0;repeat<256;++repeat)for(unsigned abi=1;abi<=4;++abi){
  ++cycles;start(abi);context->elf.psegment=heap_caps_aligned_alloc(128,4096,0);
  for(auto& f:context->files){f.open=true;++files;}
  Tab5NativeView::front=static_cast<uint16_t*>(heap_caps_aligned_alloc(16,153600,0));Tab5NativeView::back=static_cast<uint16_t*>(heap_caps_aligned_alloc(16,153600,0));
  Tab5NativeView::image.data=Tab5NativeView::front;Tab5NativeView::ready=true;Tab5NativeView::width=320;Tab5NativeView::height=240;Tab5NativeView::extendedButtons=true;Tab5NativeView::touchButtons=31;
  imageCache=heap_caps_aligned_alloc(16,153600,0);
  CHECK(appAllocate(0)==nullptr);CHECK(appAllocate(SIZE_MAX)==nullptr);
  for(unsigned slot=0;slot<32;++slot){size_t bytes=1+((slot+repeat)%511);CHECK(appAllocate(bytes)!=nullptr);}
  size_t charged=0;for(auto& item:context->allocations){CHECK(item.pointer&&item.bytes==blocks[item.pointer]&&item.bytes%16==0);charged+=item.bytes;}
  CHECK(charged==context->allocated);CHECK(appAllocate(16)==nullptr);
  for(unsigned slot=0;slot<32;slot+=2){void* p=context->allocations[slot].pointer;appRelease(p);appRelease(p);}
  charged=0;for(auto& item:context->allocations)charged+=item.bytes;CHECK(charged==context->allocated);
  failAfter=0;CHECK(appAllocate(37)==nullptr);failAfter=-1;CHECK(appAllocate(37)!=nullptr);
  CHECK(reclaim(nullptr)==0);cleanup();checkEmpty();cleanup();checkEmpty();
  // Reservations must have gone out of scope, including all failed attempts.
  Tab5Resources::Reservation reservations[8];for(auto& r:reservations){r=Tab5MemoryManager.reserve(Tab5Resources::Owner::Tools,{});CHECK(bool(r));}
  CHECK(!Tab5MemoryManager.reserve(Tab5Resources::Owner::Tools,{}));
 }
 // Abandoned launch phases: no image, partial image and per-app allocation.
 for(unsigned phase=0;phase<3;++phase){start(1);if(phase)context->elf.psegment=heap_caps_aligned_alloc(128,8192,0);if(phase==2)CHECK(appAllocate(256)!=nullptr);cleanup();checkEmpty();}
 // Console buffers are deliberately retained for output, and fully evictable.
 for(int failure=-1;failure<2;++failure){
  failAfter=failure;Tab5Resources::Budget b{};b.psram=2*ConsoleCapacity;b.psramBlock=ConsoleCapacity;
  auto admission=Tab5MemoryManager.reserve(Tab5Resources::Owner::Tools,b);CHECK(bool(admission));bool ok=createCache(admission);CHECK(ok==(failure==-1));failAfter=-1;admission.release();
  if(ok){CHECK(live==2*ConsoleCapacity);launching=true;CHECK(!reclaim(nullptr));launching=false;running=true;CHECK(!reclaim(nullptr));running=false;}
  size_t expected=live;CHECK(reclaim(nullptr)==expected);CHECK(!console&&!snapshot&&consoleLength==0&&snapshotRevision==UINT32_MAX);checkEmpty();
 }
 std::printf("{\"passed\":true,\"checks\":%u,\"cycles\":%u,\"allocationFailuresInjected\":%u,\"trackedPeakBytes\":%zu,\"trackedRetainedBytes\":%zu,\"filesRetained\":%u,\"storageLeasesRetained\":%u,\"imageCacheDrops\":%u}\n",checks,cycles,failures,peak,live,files,storage,cacheDrops);
}
