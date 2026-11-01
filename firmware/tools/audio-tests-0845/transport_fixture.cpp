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
 bool startsWith(const char*p)const{return v.rfind(p,0)==0;}size_t length()const{return v.size();}const char*c_str()const{return v.c_str();}
 void toLowerCase(){for(auto&c:v)c=std::tolower((unsigned char)c);}int indexOf(const char*p)const{auto n=v.find(p);return n==v.npos?-1:int(n);}long toInt()const{return std::strtol(v.c_str(),nullptr,10);}
};
static std::atomic<uint32_t> simulatedMs{0},yieldCount{0};static const auto startTime=std::chrono::steady_clock::now();
uint32_t millis(){return simulatedMs+uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-startTime).count());}
void vTaskDelay(unsigned n){++yieldCount;simulatedMs+=n;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
constexpr uint32_t pdMS_TO_TICKS(uint32_t n){return n;}constexpr uint32_t portMAX_DELAY=~0U;constexpr int pdTRUE=1,pdPASS=1;
struct Debug{template<class...A>void printf(const char*,A...){}void flush(){}}Tab5DebugSerial;
#define TAB5_LOG(...) ((void)0)
constexpr uint32_t MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_INTERNAL=4;
std::atomic<unsigned>allocations{0};void*heap_caps_malloc(size_t n,unsigned){auto p=std::malloc(n);if(p)++allocations;return p;}void heap_caps_free(void*p){if(p)--allocations;std::free(p);}
size_t heap_caps_get_free_size(unsigned){return 1024*1024;}size_t heap_caps_get_largest_free_block(unsigned){return 1024*1024;}
namespace Tab5Resources{size_t radioCapacity(size_t,size_t){return 64*1024;}size_t radioPreload(size_t){return 2048;}}
static std::atomic<bool>networkEnabled{true},commandPending{false};bool tab5NetworkEnabled(){return networkEnabled;}bool audioCommandPending(){return commandPending;}
constexpr int WL_CONNECTED=3;struct{int state=WL_CONNECTED;int status(){return state;}}WiFi;
struct{int wifiRssi(){return -40;}}Tab5Audio;
enum class Tab5RadioNetworkStage{Buffering};void tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage){}
class Stream{
 public:virtual ~Stream()=default;virtual int read()=0;virtual int read(uint8_t*,size_t)=0;
 void setTimeout(uint32_t n){_timeout=n;}int timedRead();uint32_t _startMillis=0,_timeout=1000;
};
// SDK_STREAM_METHODS
std::atomic<unsigned>clients{0};std::atomic<bool>peerConnected{false};bool beginAllowed=true;int responseCode=200;bool fragmentedHeaders=false,cancelWhileOpening=false;
String transferEncoding="",icyInterval="";std::vector<uint8_t>body;std::atomic<unsigned>rawReads{0};
struct NetworkClient:Stream{
 size_t pos=0;NetworkClient(){++clients;}virtual~NetworkClient(){--clients;}
 virtual int available(){return int(body.size()-pos);}virtual uint8_t connected(){return peerConnected;}
 int read(uint8_t*out,size_t n)override{++rawReads;if(fragmentedHeaders){++simulatedMs;return -1;}n=std::min(n,body.size()-pos);if(n){std::memcpy(out,body.data()+pos,n);pos+=n;}return int(n);}
 int read()override{uint8_t b;return read(&b,1)==1?b:-1;}
 virtual void stop(){peerConnected=false;}virtual void clear(){pos=body.size();}
};
struct NetworkClientSecure:NetworkClient{void setHandshakeTimeout(unsigned){}int lastError(char*b,size_t n){if(n)b[0]=0;return -1;}};
using Tab5SecureClient=NetworkClientSecure;
// PRODUCTION_COOPERATE
// PRODUCTION_CLIENT
#define log_d(...) ((void)0)
#define log_w(...) ((void)0)
constexpr int HTTPC_STRICT_FOLLOW_REDIRECTS=1;
class HTTPClient{
 public:NetworkClient*_client=nullptr;bool _reuse=false,_canReuse=false;std::unique_ptr<int>_tcpDeprecated,_transportTraits;
 ~HTTPClient();void end();void disconnect(bool);bool connected();NetworkClient*getStreamPtr();void clear(){}
 bool begin(NetworkClient&c,const String&){_client=&c;c.pos=0;return beginAllowed;}
 void useHTTP10(bool x){_reuse=!x;}void setFollowRedirects(int){}void setConnectTimeout(int){}void setTimeout(unsigned n){if(_client)_client->setTimeout(n);}void setUserAgent(const char*){}void collectHeaders(const char**,size_t){}void addHeader(const char*,const char*){}
 int GET(){peerConnected=responseCode==200;if(fragmentedHeaders){_client->setTimeout(64);if(cancelWhileOpening)commandPending=true;_client->timedRead();}return responseCode;}
 String header(const char*k){return std::strcmp(k,"icy-metaint")==0?icyInterval:transferEncoding;}
 static String errorToString(int){return "test-error";}
};
// SDK_HTTP_METHODS
class AudioFileSource{
 public:virtual~AudioFileSource()=default;virtual bool open(const char*)=0;virtual uint32_t read(void*,uint32_t)=0;virtual uint32_t readNonBlock(void*p,uint32_t n){return read(p,n);}virtual bool seek(int32_t,int)=0;virtual bool close()=0;virtual bool isOpen()=0;virtual uint32_t getSize()=0;virtual uint32_t getPos()=0;virtual bool loop(){return true;}
};
// PRODUCTION_SOURCE
struct Semaphore{std::mutex m;std::condition_variable cv;bool ready=false;};using SemaphoreHandle_t=Semaphore*;
SemaphoreHandle_t xSemaphoreCreateBinary(){return new Semaphore;}
int xSemaphoreTake(SemaphoreHandle_t s,uint32_t ms){std::unique_lock<std::mutex>g(s->m);if(ms==portMAX_DELAY)s->cv.wait(g,[&]{return s->ready;});else if(!s->cv.wait_for(g,std::chrono::milliseconds(ms),[&]{return s->ready;}))return 0;s->ready=false;return pdTRUE;}
void xSemaphoreGive(SemaphoreHandle_t s){std::lock_guard<std::mutex>g(s->m);s->ready=true;s->cv.notify_all();}void vSemaphoreDelete(SemaphoreHandle_t s){delete s;}
using TaskHandle_t=void*;int g_networkTaskCore=1;constexpr unsigned NETWORK_TASK_PRIORITY=4;std::vector<std::thread>threads;
int xTaskCreatePinnedToCore(void(*f)(void*),const char*,uint32_t,void*arg,unsigned,TaskHandle_t*h,int){*h=(void*)1;threads.emplace_back([=]{f(arg);});return pdPASS;}void vTaskDelete(TaskHandle_t){}uint32_t uxTaskGetStackHighWaterMark(TaskHandle_t){return 8192;}
struct StaticStreamBuffer_t{std::mutex m;std::deque<uint8_t>q;size_t cap=0;};using StreamBufferHandle_t=StaticStreamBuffer_t*;
StreamBufferHandle_t xStreamBufferCreateStatic(size_t cap,size_t,uint8_t*,StaticStreamBuffer_t*c){c->cap=cap-1;return c;}
void vStreamBufferDelete(StreamBufferHandle_t c){std::lock_guard<std::mutex>g(c->m);c->q.clear();}
size_t xStreamBufferBytesAvailable(StreamBufferHandle_t c){std::lock_guard<std::mutex>g(c->m);return c->q.size();}
size_t xStreamBufferSpacesAvailable(StreamBufferHandle_t c){std::lock_guard<std::mutex>g(c->m);return c->cap-c->q.size();}
size_t xStreamBufferSend(StreamBufferHandle_t c,const uint8_t*p,size_t n,unsigned){std::lock_guard<std::mutex>g(c->m);n=std::min(n,c->cap-c->q.size());for(size_t i=0;i<n;++i)c->q.push_back(p[i]);return n;}
size_t xStreamBufferReceive(StreamBufferHandle_t c,uint8_t*p,size_t n,unsigned){std::lock_guard<std::mutex>g(c->m);n=std::min(n,c->q.size());for(size_t i=0;i<n;++i){p[i]=c->q.front();c->q.pop_front();}return n;}
// PRODUCTION_BUFFERING
void reset(){assert(clients==0&&allocations==0);beginAllowed=true;responseCode=200;fragmentedHeaders=false;cancelWhileOpening=false;commandPending=false;networkEnabled=true;WiFi.state=WL_CONNECTED;body.clear();transferEncoding="";icyInterval="";peerConnected=false;}
int main(){
 unsigned checks=0;reset();
 // BASELINE_SCENARIO
 for(unsigned i=0;i<200;++i){
  reset();{Tab5HttpAudioSource source;assert(source.open(i%2?"https://radio":"http://radio"));++checks;peerConnected=false;source.close();source.close();++checks;}
  assert(clients==0);++checks;
 }
 for(int code:{-1,0,403,404,500}){reset();responseCode=code;Tab5HttpAudioSource source;assert(!source.open("https://radio"));source.close();assert(clients==0);++checks;}
 reset();beginAllowed=false;{Tab5HttpAudioSource source;assert(!source.open("http://radio"));}assert(clients==0);++checks;
 reset();transferEncoding="chunked";{Tab5HttpAudioSource source;assert(!source.open("https://radio"));}assert(clients==0);++checks;
 // Normal reads and ICY metadata stripping use the complete real source.
 reset();body={1,2,3,4,5};{Tab5HttpAudioSource source;assert(source.open("http://radio"));uint8_t out[5]{};assert(source.read(out,5)==5);assert(std::memcmp(out,body.data(),5)==0);commandPending=true;body.push_back(6);assert(source.read(out,1)==1&&out[0]==6);++checks;}assert(clients==0);
 reset();icyInterval="2";body={1,2,1};body.insert(body.end(),16,0x55);body.push_back(3);body.push_back(4);
 {Tab5HttpAudioSource source;assert(source.open("https://radio"));uint8_t out[4]{};assert(source.read(out,4)==4);assert(out[0]==1&&out[1]==2&&out[2]==3&&out[3]==4);++checks;}assert(clients==0);
 // Bare Stream::timedRead busy-polls; the actual cooperative transport yields.
 reset();fragmentedHeaders=true;yieldCount=0;{Tab5HttpAudioSource source;assert(source.open("http://radio"));assert(yieldCount>0);++checks;}assert(clients==0);
 reset();fragmentedHeaders=true;cancelWhileOpening=true;{Tab5HttpAudioSource source;assert(!source.open("https://radio"));++checks;}assert(clients==0);
 // Complete buffered producer/consumer, not just the transport close helper.
 for(unsigned cycle=0;cycle<100;++cycle){
  reset();body.resize(32768);for(size_t i=0;i<body.size();++i)body[i]=uint8_t(i);
  {Tab5BufferedRadioSource source;assert(source.open(cycle%2?"https://radio":"http://radio"));std::vector<uint8_t>out(4096);assert(source.read(out.data(),out.size())>0);source.close();source.close();++checks;}
  for(auto&t:threads)t.join();threads.clear();assert(clients==0&&allocations==0);++checks;
 }
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"radioTransportCycles\":200,\"bufferedProducerCycles\":100,\"remainingClients\":"<<clients.load()<<",\"remainingTrackedAllocations\":"<<allocations.load()<<"}\n";
}
