#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
class String{
 std::string v;
 public:String()=default;String(const char*p):v(p?p:""){}String(std::string x):v(std::move(x)){}
 friend bool operator==(const String&a,const String&b){return a.v==b.v;}friend String operator+(const String&a,const String&b){return a.v+b.v;}bool startsWith(const char*p)const{return v.rfind(p,0)==0;}size_t length()const{return v.size();}const char*c_str()const{return v.c_str();}
 void toLowerCase(){for(auto&c:v)c=std::tolower((unsigned char)c);}int indexOf(const char*p)const{auto n=v.find(p);return n==v.npos?-1:int(n);}long toInt()const{return std::strtol(v.c_str(),nullptr,10);}
};
using TickType_t=uint32_t;
static thread_local bool receiverThread=false;
static std::atomic<bool> openingPhase{false};
static std::atomic<unsigned> openingPolls{0},startNotifications{0};

#define TEST_EVENT_WAITS 1
static std::atomic<uint32_t> simulatedMs{0},yieldCount{0};static const auto startTime=std::chrono::steady_clock::now();
uint32_t millis(){return simulatedMs+uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-startTime).count());}
void vTaskDelay(unsigned n){if(receiverThread&&openingPhase)++openingPolls;++yieldCount;simulatedMs+=n;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
constexpr uint32_t pdMS_TO_TICKS(uint32_t n){return n;}constexpr uint32_t portMAX_DELAY=~0U;constexpr int pdTRUE=1,pdPASS=1;
struct Debug{template<class...A>void printf(const char*,A...){}void flush(){}}Tab5DebugSerial;
#define TAB5_LOG(...) ((void)0)
constexpr uint32_t MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_INTERNAL=4;
static size_t allocationLimit=300000;std::atomic<unsigned>allocations{0};void*heap_caps_malloc(size_t n,unsigned){if(n>allocationLimit)return nullptr;auto p=std::malloc(n);if(p)++allocations;return p;}void heap_caps_free(void*p){if(p)--allocations;std::free(p);}
size_t heap_caps_get_free_size(unsigned){return 24*1024*1024;}size_t heap_caps_get_largest_free_block(unsigned){return 1024*1024;}
#include "Tab5ResourceManager.h"
static std::atomic<bool>networkEnabled{true},commandPending{false};bool tab5NetworkEnabled(){return networkEnabled;}bool audioCommandPending(){return commandPending;}
constexpr int WL_CONNECTED=3;struct{int state=WL_CONNECTED;int status(){return state;}void setSleep(bool){}}WiFi;

enum class Tab5RadioNetworkStage{Buffering,HttpOpen,DecoderStart,Playing};void tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage){}

class AudioFileSource{public:virtual~AudioFileSource()=default;virtual bool open(const char*)=0;virtual uint32_t read(void*,uint32_t)=0;virtual uint32_t readNonBlock(void*p,uint32_t n){return read(p,n);}virtual bool seek(int32_t,int)=0;virtual bool close()=0;virtual bool isOpen()=0;virtual uint32_t getSize()=0;virtual uint32_t getPos()=0;virtual bool loop(){return true;}};
static bool upstreamAllowed=true;static unsigned upstreamOpen=0;
struct Tab5HttpAudioSource{bool opened=false;std::atomic<bool>*abort=nullptr;void setAbortFlag(std::atomic<bool>*p){abort=p;}bool open(const char*){opened=upstreamAllowed;if(opened)++upstreamOpen;return opened;}bool close(){if(opened)--upstreamOpen;opened=false;return true;}bool isOpen(){return opened;}uint32_t read(void*p,size_t n){if(!opened||(abort&&*abort))return 0;memset(p,0x56,n);return n;}};
struct Semaphore{std::mutex m;std::condition_variable cv;bool ready=false;};using SemaphoreHandle_t=Semaphore*;
SemaphoreHandle_t xSemaphoreCreateBinary(){return new Semaphore;}
int xSemaphoreTake(SemaphoreHandle_t s,uint32_t ms){std::unique_lock<std::mutex>g(s->m);if(ms==portMAX_DELAY)s->cv.wait(g,[&]{return s->ready;});else if(!s->cv.wait_for(g,std::chrono::milliseconds(ms),[&]{return s->ready;}))return 0;s->ready=false;return pdTRUE;}
void xSemaphoreGive(SemaphoreHandle_t s){std::lock_guard<std::mutex>g(s->m);s->ready=true;s->cv.notify_all();}void vSemaphoreDelete(SemaphoreHandle_t s){delete s;}
struct Task {std::mutex m;std::condition_variable cv;unsigned notifications=0;bool exited=false;};
using TaskHandle_t=Task*;static thread_local Task*currentTask=nullptr;
int g_networkTaskCore=1;constexpr unsigned NETWORK_TASK_PRIORITY=4;std::vector<std::thread>threads;std::vector<std::unique_ptr<Task>>taskControls;
int xTaskCreatePinnedToCore(void(*f)(void*),const char*,uint32_t,void*arg,unsigned,TaskHandle_t*h,int){taskControls.emplace_back(new Task);*h=taskControls.back().get();Task*t=*h;threads.emplace_back([=]{currentTask=t;receiverThread=true;f(arg);});return pdPASS;}
void vTaskDelete(TaskHandle_t){std::lock_guard<std::mutex>g(currentTask->m);currentTask->exited=true;}
void xTaskNotifyGive(TaskHandle_t t){assert(t);std::lock_guard<std::mutex>g(t->m);assert(!t->exited);++t->notifications;++startNotifications;t->cv.notify_all();}
uint32_t ulTaskNotifyTake(int clear,uint32_t timeout){assert(currentTask);std::unique_lock<std::mutex>g(currentTask->m);auto ready=[&]{return currentTask->notifications>0;};if(timeout==portMAX_DELAY)currentTask->cv.wait(g,ready);else if(!currentTask->cv.wait_for(g,std::chrono::milliseconds(timeout),ready))return 0;auto count=currentTask->notifications;if(clear)currentTask->notifications=0;else --currentTask->notifications;return count;}
uint32_t uxTaskGetStackHighWaterMark(TaskHandle_t){return 8192;}
struct StaticStreamBuffer_t{std::mutex m;std::condition_variable readable,writable;std::deque<uint8_t>q;size_t cap=0;};using StreamBufferHandle_t=StaticStreamBuffer_t*;
StreamBufferHandle_t xStreamBufferCreateStatic(size_t cap,size_t,uint8_t*,StaticStreamBuffer_t*c){c->cap=cap-1;return c;}
void vStreamBufferDelete(StreamBufferHandle_t c){std::lock_guard<std::mutex>g(c->m);c->q.clear();}
size_t xStreamBufferBytesAvailable(StreamBufferHandle_t c){std::lock_guard<std::mutex>g(c->m);return c->q.size();}
size_t xStreamBufferSpacesAvailable(StreamBufferHandle_t c){std::lock_guard<std::mutex>g(c->m);return c->cap-c->q.size();}
size_t xStreamBufferSend(StreamBufferHandle_t c,const uint8_t*p,size_t n,unsigned timeout){std::unique_lock<std::mutex>g(c->m);if(timeout)c->writable.wait_for(g,std::chrono::milliseconds(timeout),[&]{return c->cap-c->q.size()>=n;});n=std::min(n,c->cap-c->q.size());for(size_t i=0;i<n;++i)c->q.push_back(p[i]);c->readable.notify_one();return n;}
size_t xStreamBufferReceive(StreamBufferHandle_t c,uint8_t*p,size_t n,unsigned timeout){std::unique_lock<std::mutex>g(c->m);if(timeout)c->readable.wait_for(g,std::chrono::milliseconds(timeout),[&]{return !c->q.empty();});n=std::min(n,c->q.size());for(size_t i=0;i<n;++i){p[i]=c->q.front();c->q.pop_front();}c->writable.notify_one();return n;}

// PRODUCTION_BUFFERING
struct Output{};
enum class Tab5AudioClient{None,Audio};
struct{struct SpeakerType{bool running=false;struct Config{unsigned dma_buf_len=256,dma_buf_count=8;};Config config(){return{};}bool isRunning(){return running;}}Speaker;}M5;
struct Device{Tab5AudioClient held=Tab5AudioClient::None;Tab5AudioClient owner(){return held;}void setVolume(uint8_t){}bool claim(Tab5AudioClient c,unsigned){held=c;M5.Speaker.running=true;return true;}}Tab5AudioDevice;
struct Decoder{Decoder(void*,size_t){}static size_t preAllocSize(){return 30000;}bool begin(AudioFileSource*,Output*){return true;}};
using AudioGeneratorMP3=Decoder;using Tab5SeekableAacGenerator=Decoder;
static size_t aacWorkspaceBytes(){return 180000;}
void tab5NetDiagnosticsService(){}void tab5LogHeap(const char*){}void tab5NetworkSetMediaPending(bool){}
struct Tab5NetworkLease{Tab5NetworkLease(const char*,unsigned,bool(*)(void*)){}explicit operator bool()const{return true;}void shareAfterSetup(){}};
static Tab5Resources::Budget requested{};
static Tab5Resources::MemorySample sample(){return {200000,100000,100000,24*1024*1024,allocationLimit};}
static Tab5Resources::MemoryTotals totals(){return {200000,24*1024*1024};}
static Tab5Resources::Manager manager(sample,totals);
Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner owner,const Tab5Resources::Budget&b,const char*,Tab5Resources::AdmissionReport*r){requested=b;return manager.reserve(owner,b,r);}
enum class Tab5AudioMode{Idle,Radio};struct Tab5AudioStation{String name,style,url;};
class Tab5AudioEngine{public:
 class StateGuard{public:StateGuard(Tab5AudioEngine&){}explicit operator bool()const{return true;}};
 Output*_output=nullptr;std::vector<Tab5AudioStation>_stations;String _selectedRadioUrl,_stationCategory,_currentLocalPath;size_t _stationIndex=0;
 bool _radioStartCancelled=false,_paused=false,_playingRecording=false,_radioReconnectPending=false;uint32_t _localEstimatedSeconds=0,_playStartedAt=0,_pauseStartedAt=0,_radioReconnectAt=0;uint8_t _volume=125;
 Tab5NetworkLease*_radioLease=nullptr;AudioFileSource*_streamSource=nullptr;Tab5BufferedRadioSource*_radioSource=nullptr;Decoder*_mp3=nullptr,*_aac=nullptr;void*_aacCodecBuffer=nullptr,*_mp3CodecBuffer=nullptr;size_t _mp3CodecBufferSize=30000;Tab5AudioMode _mode=Tab5AudioMode::Idle;
 uint32_t stationCatalogueRevision(){return 1;}void setStatus(const String&){}void setNowPlaying(const String&){}
 bool ensureMp3CodecBuffer(){return true;}bool ensureAacCodecBuffer(){return true;}
 void cleanupDecoder(bool preserve=false){delete _mp3;_mp3=nullptr;delete _aac;_aac=nullptr;delete _streamSource;_streamSource=nullptr;_radioSource=nullptr;delete _radioLease;_radioLease=nullptr;if(!preserve){M5.Speaker.running=false;Tab5AudioDevice.held=Tab5AudioClient::None;}}
 void stopPlayback(bool,bool preserve=false){cleanupDecoder(preserve);}bool startRadioNow(size_t,uint32_t=0);~Tab5AudioEngine(){cleanupDecoder();}
};
// PRODUCTION_START
void joinThreads(){for(auto&t:threads)t.join();threads.clear();taskControls.clear();}
int main(){
 unsigned checks=0;Output output;Tab5AudioEngine e;e._output=&output;e._stations={{"MP3","test","http://radio"},{"AAC","test","https://radio/aac.aac"}};
 for(unsigned cycle=0;cycle<25;++cycle){
  allocationLimit=300000;assert(e.startRadioNow(0));++checks;
  assert(e._radioSource->capacityBytes()==256*1024);assert(requested.psramBlock==64*1024+1);assert(requested.psram<2*1024*1024);checks+=3;
  const auto cold=requested;assert(e.startRadioNow(1));
  assert(e._radioSource->capacityBytes()==256*1024);assert(requested.psramBlock==aacWorkspaceBytes());assert(requested.dmaBlock==0);checks+=3;
  // Same requested codec/network policy with existing I2S reduces real startup charge.
  const auto warm=Tab5Resources::radioBudget(false,1024*1024,30000,true);
  assert(cold.internal-warm.internal==12*1024);assert(cold.psram-warm.psram==12*1024);checks+=2;
  e.cleanupDecoder();joinThreads();assert(allocations==0&&upstreamOpen==0);++checks;
  allocationLimit=65537;assert(e.startRadioNow(0));assert(e._radioSource->capacityBytes()==65536);checks+=2;e.cleanupDecoder();joinThreads();assert(allocations==0);++checks;
  allocationLimit=65536;assert(!e.startRadioNow(0));assert(!e._radioSource&&allocations==0&&upstreamOpen==0);checks+=2;
  allocationLimit=300000;upstreamAllowed=false;assert(!e.startRadioNow(0));joinThreads();assert(allocations==0&&upstreamOpen==0);checks+=2;upstreamAllowed=true;
 }
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"cycles\":25,\"ringFallbackBytes\":65536,\"outstandingAllocations\":"<<allocations<<",\"upstreamConnections\":"<<upstreamOpen<<"}"<<std::endl;
}
