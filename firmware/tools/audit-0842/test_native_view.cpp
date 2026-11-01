#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_set>
#include "Tab5AppInputQueue.h"
using Clock=std::chrono::steady_clock;
using StaticSemaphore_t=std::mutex;
using SemaphoreHandle_t=std::mutex*;
constexpr uint32_t portMAX_DELAY=UINT32_MAX;
constexpr int pdTRUE=1,MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* m){return m;}
int xSemaphoreTake(SemaphoreHandle_t m,uint32_t wait){if(wait==0)return m->try_lock()?1:0;m->lock();return 1;}
void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
std::unordered_set<void*> allocations;
int failAfter=-1;
void* heap_caps_malloc(size_t n,int){if(failAfter==0)return nullptr;if(failAfter>0)--failAfter;void* p=std::malloc(n);if(p)allocations.insert(p);return p;}
void* heap_caps_calloc(size_t n,size_t size,int caps){void* p=heap_caps_malloc(n*size,caps);if(p)std::memset(p,0,n*size);return p;}
void heap_caps_free(void* p){if(p){assert(allocations.erase(p)==1);std::free(p);}}
namespace Tab5Resources {enum class Owner{Tools};struct Budget {size_t psram=0,psramBlock=0;};}
struct Admission {explicit operator bool()const{return true;}void allocated(size_t,size_t){}};
Admission tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Budget,const char*){return {};}
struct lv_obj_t{};
struct Image {uint8_t* data=nullptr;};
struct Focus {void clear(){}} Tab5Focus;
lv_obj_t* lv_screen_active(){return nullptr;}
bool lv_obj_is_valid(lv_obj_t*){return false;}
void lv_screen_load(lv_obj_t*){}
void lv_obj_delete(lv_obj_t*){}
void lv_image_cache_drop(Image*){}
uint32_t tick=100;
uint32_t lv_tick_get(){return tick;}
unsigned published=0,invalidated=0;
void lv_image_set_src(lv_obj_t*,Image*){++published;}
void lv_obj_invalidate(lv_obj_t*){++invalidated;}
void lv_label_set_text(lv_obj_t*,const char*){}
const char* tr(const char* s){return s;}
std::atomic<bool> ready{false},configuring{false};
std::atomic<uint32_t> touchButtons{0},requests{0};
uint16_t *front=nullptr,*back=nullptr;
unsigned width=0,height=0;
bool dirty=false,extendedButtons=false,canvasMode=false;
Tab5AppInputQueue* input=nullptr;
uint32_t received=0,shown=0,lastStats=0;
lv_obj_t *screen=nullptr,*previous=nullptr,*picture=nullptr,*fpsLabel=nullptr,*volumeSlider=nullptr,*volumeLabel=nullptr,*controls=nullptr,*padLabel=nullptr,*padConfigure=nullptr;
int padStep=-1;
Image image{};
StaticSemaphore_t lockStorage,inputLockStorage;
std::mutex copyMutex;
std::condition_variable copyCv;
bool holdCopy=false,copyEntered=false;
void copyFrame(void* dst,const void* src,size_t size){
  std::unique_lock<std::mutex> guard(copyMutex);
  if(holdCopy){copyEntered=true;copyCv.notify_all();copyCv.wait(guard,[]{return !holdCopy;});}
  guard.unlock();std::memcpy(dst,src,size);
}
#include "production.inc"

template<class Operation> double contend(Operation operation,const uint16_t* pixels,size_t count){
  {std::lock_guard<std::mutex> guard(copyMutex);holdCopy=true;copyEntered=false;}
  std::thread writer([&]{assert(present(pixels,count));});
  {std::unique_lock<std::mutex> guard(copyMutex);copyCv.wait(guard,[]{return copyEntered;});}
  std::thread release([]{std::this_thread::sleep_for(std::chrono::milliseconds(80));std::lock_guard<std::mutex> guard(copyMutex);holdCopy=false;copyCv.notify_all();});
  auto started=Clock::now();operation();
  double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
  release.join();writer.join();return elapsed;
}
int main(int argc,char** argv){
  const bool baseline=argc>1&&!std::strcmp(argv[1],"baseline");
  // Every partial allocation must roll back; successful reopen/close returns
  // both image buffers and the bounded input queue, including repeated cycles.
  for(unsigned cycle=0;cycle<128;++cycle){
    for(int failure=0;failure<3;++failure){failAfter=failure;assert(!beginMode(800,480,true));assert(allocations.empty());assert(!ready);}
    failAfter=-1;assert(beginMode(800,480,true));assert(allocations.size()==3);
    close();assert(allocations.empty());assert(!ready&&front==nullptr&&back==nullptr&&input==nullptr);
  }
  assert(beginMode(800,480,true));
  uint16_t* pixels=new uint16_t[800*480];std::fill(pixels,pixels+800*480,uint16_t(0x39ab));
  tick+=100;unsigned before=published;
  double frameMs=contend([]{serviceFrame();},pixels,800*480);
  if(baseline){assert(frameMs>=50);assert(published==before+1);}
  else {assert(frameMs<30);assert(published==before);assert(dirty);}
  tick+=100;serviceFrame();assert(!dirty);assert(image.data==reinterpret_cast<uint8_t*>(front));
  for(size_t i=0;i<800*480;++i)assert(front[i]==0x39ab);
  tab5_app_input_event event{};event.size=sizeof(event);event.type=TAB5_INPUT_KEY;event.key=TAB5_KEY_ENTER;
  double inputMs=contend([&]{queueInput(event);},pixels,800*480);
  if(baseline)assert(inputMs>=50);else assert(inputMs<30);
  tab5_app_input_event output{};output.size=sizeof(output);assert(inputPoll(&output)==1&&output.type==event.type&&output.key==event.key);assert(inputPoll(&output)==0);
  // Ordered non-move events survive the independent queue; movement coalescing
  // and the overload cancel policy are provided by the production queue class.
  for(unsigned i=0;i<20;++i){event.key=100+i;queueInput(event);}
  for(unsigned i=0;i<20;++i){assert(inputPoll(&output)==1);assert(output.key==100+i);}
  assert(inputPoll(&output)==0);assert(!present(pixels,1));
  delete[] pixels;close();assert(allocations.empty());
  std::printf("%s: injected 80 ms frame-copy contention: UI service %.3f ms; input %.3f ms; 128 reopen/close + 384 allocation failures, no outstanding allocations\n",baseline?"baseline":"fixed",frameMs,inputMs);
}
