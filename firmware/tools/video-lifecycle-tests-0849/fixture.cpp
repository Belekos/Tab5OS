#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
class String {
 std::string value;
 public:String()=default;String(const char* s):value(s?s:""){}String(std::string s):value(std::move(s)){}
 size_t length()const{return value.size();}const char* c_str()const{return value.c_str();}
 void toLowerCase(){for(char& c:value)c=std::tolower(static_cast<unsigned char>(c));}
 bool operator==(const char* rhs)const{return value==rhs;}bool operator==(const String& rhs)const{return value==rhs.value;}
 bool operator!=(const char* rhs)const{return !(*this==rhs);}
};
using BaseType_t=int;using TickType_t=uint32_t;using TaskHandle_t=void*;
using SemaphoreHandle_t=std::timed_mutex*;
constexpr int pdTRUE=1,pdPASS=1;constexpr uint32_t portMAX_DELAY=0xffffffff;
uint32_t pdMS_TO_TICKS(uint32_t t){return t;}
void beforeLock(){}
int xSemaphoreTake(SemaphoreHandle_t m,TickType_t wait){beforeLock();if(wait==portMAX_DELAY){m->lock();return 1;}return m->try_lock_for(std::chrono::milliseconds(wait));}
void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
std::atomic<unsigned> deletedTasks{0};
void vTaskDelete(void*){++deletedTasks;}
// A UI join would fail this test rather than actually sleeping 5.5 seconds.
void vTaskDelay(int){}
uint32_t millis(){return 0;}
uint64_t esp_timer_get_time(){return 9000;}
template<class T>T constrain(T n,T a,T b){return std::min(b,std::max(n,a));}
bool isRemoteSource(const String& s){return std::string(s.c_str()).rfind("http",0)==0;}
bool isUnsupportedContainerPath(const String& s){return std::string(s.c_str()).find(".mp4")!=std::string::npos;}
bool isVideoPath(const String& s){auto t=std::string(s.c_str());return t.find(".avi")!=std::string::npos||t.find(".mjpeg")!=std::string::npos;}
String sourceName(const String& s){return s;}
namespace Tab5Resources{enum class Owner{Video};struct TaskBudget{explicit TaskBudget(unsigned){}};struct Budget{size_t a,b,c,d,e;};}
std::atomic<bool> memoryAllowed{true};
struct Admission{explicit operator bool()const{return memoryAllowed;}};
Admission tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Budget,const char*){return {};}
Admission tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::TaskBudget,const char*){return {};}
void tab5DiagnosticMark(int,const char*,const char*,int){}
void tab5TaskCheckpoint(const char*){}
std::atomic<bool> storageAllowed{true};
struct Tab5StorageUse{explicit Tab5StorageUse(bool){}operator bool()const{return storageAllowed;}};

#include <unordered_map>
#include <sanitizer/asan_interface.h>
#include "Tab5JpegHeader.h"
constexpr size_t MAX_JPEG_BYTES=2*1024*1024,MAX_FRAME_BYTES=1280*720*2;
constexpr unsigned MAX_VIDEO_WIDTH=1280,MAX_VIDEO_HEIGHT=720;
constexpr unsigned MALLOC_CAP_SPIRAM=1024;
using esp_err_t=int;constexpr int ESP_OK=0;
std::unordered_map<void*,size_t> buffers;int failAllocation=-1,allocationAttempts=0;
size_t copiedBytes=0,zeroedBytes=0;
void* allocate(size_t n,size_t alignment,bool zero){
 if(allocationAttempts++==failAllocation)return nullptr;
 void* p=nullptr;if(posix_memalign(&p,alignment,n))return nullptr;
 buffers[p]=n;if(zero){memset(p,0,n);zeroedBytes+=n;}else __asan_poison_memory_region(p,n);
 return p;
}
void* heap_caps_malloc(size_t n,unsigned caps){assert(caps==MALLOC_CAP_SPIRAM);return allocate(n,16,false);}
void* heap_caps_aligned_alloc(size_t a,size_t n,unsigned caps){assert(caps==MALLOC_CAP_SPIRAM&&a==128&&n%128==0);return allocate(n,a,false);}
int esp_cache_get_alignment(unsigned,size_t* result){*result=128;return ESP_OK;}
void* heap_caps_calloc(size_t count,size_t n,unsigned caps){assert(caps==MALLOC_CAP_SPIRAM);return allocate(count*n,16,true);}
void* heap_caps_aligned_calloc(size_t a,size_t count,size_t n,unsigned caps){assert(caps==MALLOC_CAP_SPIRAM);return allocate(count*n,a,true);}
struct jpeg_decode_memory_alloc_cfg_t{int buffer_direction;};
constexpr int JPEG_DEC_ALLOC_OUTPUT_BUFFER=1;
#define JPEG_ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))
// NATIVE_ALLOCATOR
void release(void* p){if(!p)return;assert(buffers.count(p));__asan_unpoison_memory_region(p,buffers[p]);buffers.erase(p);free(p);}
void* copyBytes(void* dst,const void* src,size_t n){__asan_unpoison_memory_region(dst,n);copiedBytes+=n;return memcpy(dst,src,n);}
#define free release
#define memcpy copyBytes
struct jpeg_decode_engine_cfg_t{int timeout_ms;};
using jpeg_decoder_handle_t=void*;int engines=0;bool engineAllowed=true;
int jpeg_new_decoder_engine(const jpeg_decode_engine_cfg_t*,jpeg_decoder_handle_t* handle){if(!engineAllowed)return -1;*handle=reinterpret_cast<void*>(++engines);return ESP_OK;}
void jpeg_del_decoder_engine(jpeg_decoder_handle_t){--engines;}
struct jpeg_decode_cfg_t{int output_format,rgb_order,conv_std;};
constexpr int JPEG_DECODE_OUT_FORMAT_RGB565=0,JPEG_DEC_RGB_ELEMENT_ORDER_RGB=0,JPEG_YUV_RGB_CONV_STD_BT601=0;
int decodeMode=0;uint8_t* decodedDestination=nullptr;
int jpeg_decoder_process(jpeg_decoder_handle_t,const jpeg_decode_cfg_t*,const uint8_t*,size_t,uint8_t* out,size_t capacity,uint32_t* length){
 decodedDestination=out;size_t filled=decodeMode==1?capacity/2:capacity;
 __asan_unpoison_memory_region(out,filled);memset(out,0x5a,filled);*length=decodeMode==2?1:capacity;
 return decodeMode==1?-1:ESP_OK;
}
struct Logger{template<class... A>void printf(const char*,A...){}}Tab5DebugSerial;
// PRODUCTION_GUARD
#define private public
#include "Tab5Video.h"
#undef private
void Tab5VideoEngine::logMemory(const char*)const{}
bool Tab5VideoEngine::waitUntil(uint64_t){return !_stopRequested;}
using Entry=void(*)(void*);
struct Job{Entry entry;void* argument;};std::vector<Job> jobs;
bool taskCreationAllowed=true;
int tab5CreateAdmittedTask(Admission&,Entry entry,const char*,unsigned,void* argument,unsigned,TaskHandle_t* task,int){if(!taskCreationAllowed)return 0;*task=reinterpret_cast<void*>(jobs.size()+1);jobs.push_back({entry,argument});return pdPASS;}
const uint8_t smallJpeg[]={0xff,0xd8,0xff,0xc0,0,17,8,0,16,0,16,3,1,0x22,0,2,0x11,1,3,0x11,1,0xff,0xda,0,12,3,1,0,2,0x11,3,0x11,0,63,0,0,0xff,0xd9};
void Tab5VideoEngine::releasePlayback(){}
void Tab5VideoEngine::taskMain(){
 if(decodeAndPublish(smallJpeg,sizeof(smallJpeg),0))setState(Tab5VideoState::Finished,"Finished");
 struct Input{void close(){}}input;
 // PRODUCTION_CLEANUP
}
// PRODUCTION_METHODS
#undef free
#undef memcpy
unsigned checks=0;
const char* stage="decoder buffer checks";
void check(bool ok){++checks;if(!ok)std::cerr<<"FAIL check "<<checks<<": "<<stage<<std::endl;assert(ok);}
void cleanup(Tab5VideoEngine& e){e.releaseDecoderResources();e.releaseFrames();check(buffers.empty());check(engines==0);}
void initialize(uint8_t* p,size_t n,uint8_t value){__asan_unpoison_memory_region(p,n);memset(p,value,n);}
int main(){
 std::timed_mutex mutex;Tab5VideoEngine e;e._mutex=&mutex;
 jpeg_decode_memory_alloc_cfg_t nativeCfg{JPEG_DEC_ALLOC_OUTPUT_BUFFER};
 size_t nativeSize=0;for(unsigned i=0;i<3;++i){void* p=jpeg_alloc_decoder_mem(MAX_FRAME_BYTES,&nativeCfg,&nativeSize);check(p&&nativeSize==MAX_FRAME_BYTES);release(p);}
 const size_t nativeZeroed=zeroedBytes;check(nativeZeroed==5529600);zeroedBytes=0;
 // 720p triple buffers: addresses and sizes match native cache constraints.
 check(e.ensureDecoderBuffers(MAX_FRAME_BYTES));check(buffers.size()==3);check(zeroedBytes==0);check(e._frameCapacity==MAX_FRAME_BYTES);cleanup(e);
 // Empty input replacement copies no stale bytes, even if every old byte is poisoned.
 check(e.ensureJpegInput(128*1024));size_t before=copiedBytes;
 check(e.ensureJpegInput(256*1024));check(copiedBytes==before);cleanup(e);
 // Raw MJPEG growth preserves precisely the initialized prefix.
 check(e.ensureJpegInput(128*1024));initialize(e._jpegInput,128*1024,0x93);before=copiedBytes;
 check(e.ensureJpegInput(128*1024+1,128*1024));check(copiedBytes-before==128*1024);check(e._jpegInput[128*1024-1]==0x93);
 const auto* saved=e._jpegInput;failAllocation=allocationAttempts;
 check(!e.ensureJpegInput(512*1024,128*1024));check(e._jpegInput==saved);check(e._jpegInput[0]==0x93);failAllocation=-1;cleanup(e);
 // Every allocation failure cleans all partially created frame buffers.
 for(int nth=0;nth<3;++nth){failAllocation=allocationAttempts+nth;check(!e.ensureDecoderBuffers(512));check(buffers.empty());cleanup(e);failAllocation=-1;}
 engineAllowed=false;check(!e.ensureDecoderBuffers(512));cleanup(e);engineAllowed=true;
 memoryAllowed=false;check(!e.ensureDecoderBuffers(512));check(!e.ensureJpegInput(1));cleanup(e);memoryAllowed=true;
 const uint8_t jpeg[]={0xff,0xd8,0xff,0xc0,0,17,8,0,16,0,16,3,1,0x22,0,2,0x11,1,3,0x11,1,0xff,0xda,0,12,3,1,0,2,0x11,3,0x11,0,63,0,0,0xff,0xd9};
 for(unsigned cycle=0;cycle<200;++cycle){
  e._stopRequested=false;e._consecutiveDecodeErrors=0;e._frameGeneration=0;e._frontFrame=0;e._uiFrame=0xff;
  decodeMode=1;check(e.decodeAndPublish(jpeg,sizeof(jpeg),0));check(e._frameGeneration==0);
  decodeMode=2;check(!e.decodeAndPublish(jpeg,sizeof(jpeg),0));check(e._frameGeneration==0);
  decodeMode=0;check(e.decodeAndPublish(jpeg,sizeof(jpeg),0));check(e._frameGeneration==1);
  const uint8_t pinned=e._frontFrame;e._uiFrame=pinned;uint8_t* frame=e._frames[pinned];check(frame[0]==0x5a);
  check(e.decodeAndPublish(jpeg,sizeof(jpeg),0));check(decodedDestination!=frame);check(frame[0]==0x5a);
  check(!e.ensureDecoderBuffers(1024));check(e._frames[pinned]==frame);
  e._stopRequested=true;unsigned generation=e._frameGeneration;
  check(!e.decodeAndPublish(jpeg,sizeof(jpeg),0));check(e._frameGeneration==generation);cleanup(e);
 }
 // Actual entry chain: playSource -> taskEntry -> decode/publication -> service -> frame.
 // With a finished image still visible, repeated playback must reconstruct
 // missing slots, preserve the borrowed bytes, and return all work buffers.
 for(unsigned cycle=0;cycle<100;++cycle){
  e._sdAvailable=true;decodeMode=0;check(e.playSource("one.avi"));
  auto job=jobs.back();job.entry(job.argument);check(!e._taskRunning);check(engines==0);
  stage="finished video service must retain only its final frame";e.service();check(buffers.size()==1);
  Tab5VideoFrame shown;check(e.frame(shown));check(shown.data[0]==0x5a);e.service();check(buffers.size()==1);
  const auto* borrowed=shown.data;
  check(e.playSource("two.avi"));job=jobs.back();job.entry(job.argument);
  // The new final frame may not yet be adopted. Both it and the previous UI
  // image remain alive through this exact interleaving.
  e.service();check(buffers.size()==2);check(buffers.count(const_cast<uint8_t*>(borrowed))==1);check(borrowed[0]==0x5a);
  check(e.frame(shown));check(shown.data!=borrowed);e.service();check(buffers.size()==1);check(shown.data[0]==0x5a);
  borrowed=shown.data;memoryAllowed=false;check(!e.playSource("denied.avi"));e.service();
  check(buffers.size()==1&&buffers.count(const_cast<uint8_t*>(borrowed))==1);check(borrowed[0]==0x5a);memoryAllowed=true;
  taskCreationAllowed=false;check(!e.playSource("task-failed.avi"));e.service();
  check(buffers.size()==1&&buffers.count(const_cast<uint8_t*>(borrowed))==1);check(borrowed[0]==0x5a);taskCreationAllowed=true;
  // Each missing-slot failure preserves the old borrowed frame. The whole
  // decoder session still returns its engine before publication of stopped.
  for(int nth=0;nth<2;++nth){
   borrowed=shown.data;check(e.playSource("fail.avi"));failAllocation=allocationAttempts+nth;
   job=jobs.back();job.entry(job.argument);failAllocation=-1;e.service();
   check(buffers.size()==1);check(buffers.count(const_cast<uint8_t*>(borrowed))==1);check(borrowed[0]==0x5a);check(engines==0);
  }
  // UI detaches its image before calling detachDisplay, exactly as the real
  // viewer does; no retained frame remains after this public transition.
  e.stop();e.detachDisplay();check(buffers.empty());check(e._frameCapacity==0);
 }
 std::cout<<"{\"passed\":true,\"assertions\":"<<checks<<",\"decodeCycles\":200,\"publicLifecycleCycles\":100,\"outstandingBuffers\":"<<buffers.size()<<",\"outstandingEngines\":"<<engines<<",\"zeroedOutputBytes\":"<<zeroedBytes<<",\"nativeZeroedOutputBytes\":"<<nativeZeroed<<",\"first720pTripleBufferBytes\":5529600,\"rawGrowthPreservedBytes\":131072,\"unusedInputGrowthCopiedBytes\":0}"<<std::endl;
}
