#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class String {
 std::string s;
 public:
 String()=default; String(const char* p):s(p?p:""){} String(std::string p):s(std::move(p)){}
 size_t length() const{return s.size();}const char* c_str()const{return s.c_str();}
 bool startsWith(const char* p)const{return s.rfind(p,0)==0;}
 bool endsWith(const char* p)const{const size_t n=std::strlen(p);return n<=s.size()&&s.compare(s.size()-n,n,p)==0;}
 void toLowerCase(){for(auto&c:s)c=std::tolower(static_cast<unsigned char>(c));}
 int indexOf(char c)const{auto n=s.find(c);return n==s.npos?-1:int(n);}
 int indexOf(const char* p)const{auto n=s.find(p);return n==s.npos?-1:int(n);}
 void remove(size_t n){s.erase(n);}bool operator==(const char* p)const{return s==p;}
 bool operator!=(const char* p)const{return s!=p;}
};
using BaseType_t=int;using TickType_t=uint32_t;using TaskHandle_t=void*;
using SemaphoreHandle_t=std::mutex*;constexpr int pdTRUE=1;
constexpr uint32_t portMAX_DELAY=0xffffffff;
uint32_t pdMS_TO_TICKS(uint32_t t){return t;}
int xSemaphoreTake(SemaphoreHandle_t m,TickType_t){m->lock();return 1;}
void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
uint32_t clockMs=0;void vTaskDelay(uint32_t t){clockMs+=t;}
uint32_t millis(){return clockMs;}uint64_t esp_timer_get_time(){return clockMs*1000ULL;}
struct Tab5TimeSlice{bool due(uint32_t){return false;}};
constexpr uint32_t NETWORK_STALL_MS=5000;
bool isRemoteSource(const String&s){return s.startsWith("http://")||s.startsWith("https://");}
bool networkEnabled=true;bool tab5NetworkEnabled(){return networkEnabled;}
struct Debug{template<class...T>void printf(const char*,T...){}void println(const char*){}}Tab5DebugSerial;
void tab5DiagnosticMark(int,const char*,const char*,int){}
enum class Tab5VideoNetworkStage{None,HttpOpen,Streaming};
void tab5NetDiagnosticsVideoStage(Tab5VideoNetworkStage){}
void tab5NetDiagnosticsVideoEnd(){}
namespace Tab5Resources{
 enum class Owner{Video};struct Budget{size_t a,b,c,d,e;};
 Budget tlsBudget(){return{};}
}
struct Admission{explicit operator bool()const{return true;}void release(){}};
Admission tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Budget,const char*){return{};}
std::vector<std::string> events;bool priorityAllowed=true;std::atomic<bool>* stopDuringPriority=nullptr;
struct Tab5AudioPriorityLease{
 bool held=false;
 bool tryAcquire(uint32_t=7000){events.push_back("priority-acquire");if(stopDuringPriority)*stopDuringPriority=true;return held=priorityAllowed;}
 explicit operator bool()const{return held;}
 ~Tab5AudioPriorityLease(){if(held)events.push_back("priority-release");}
};
enum class Tab5AudioMode{None,Local,Radio,Recording};
struct Tab5AudioSnapshot{Tab5AudioMode mode=Tab5AudioMode::None;};
Tab5AudioMode audioMode=Tab5AudioMode::None;
struct Audio{bool snapshot(Tab5AudioSnapshot&r){r.mode=audioMode;return true;}}Tab5Audio;
enum class Tab5PcmFormat{U8,S16,S24,S32};
struct Tab5PcmSpec{uint32_t sampleRate=0;uint8_t channels=0;Tab5PcmFormat format=Tab5PcmFormat::S16;};
enum class Tab5AudioClient{Video};
int audioClaims=0,audioWrites=0;
struct AudioDevice{
 bool claim(Tab5AudioClient,int){++audioClaims;return true;}
 bool configure(Tab5AudioClient,const Tab5PcmSpec&){return true;}
 bool writePcm(Tab5AudioClient,const uint8_t*,size_t,const Tab5PcmSpec&){++audioWrites;return true;}
 void release(Tab5AudioClient){}void flush(Tab5AudioClient){}
}Tab5AudioDevice;
int leases=0;
struct Tab5NetworkLease{
 Tab5NetworkLease(const char*,int,bool(*)(void*),void*){++leases;events.push_back("network-lease");}
 ~Tab5NetworkLease(){--leases;events.push_back("network-release");}
 explicit operator bool()const{return true;}
};
int clients=0,clientStops=0,httpDestructions=0,opens=0;
bool transportConnected=false,streamMissing=false;int responseCode=200;bool beginAllowed=true;
struct NetworkClient{
 NetworkClient(){++clients;}virtual ~NetworkClient(){--clients;events.push_back("client-delete");}
 virtual int available(){return 0;}virtual uint8_t connected(){return transportConnected;}
 virtual int read(uint8_t*,size_t){return 0;}
 virtual void stop(){++clientStops;transportConnected=false;events.push_back("client-stop");}
 virtual void clear(){}
};
struct Tab5SecureClient:NetworkClient{void setHandshakeTimeout(unsigned){}};
template<class T>struct Tab5CooperativeClient:T{
 Tab5CooperativeClient(bool(*)(void*),void*){}
};
#define log_d(...) ((void)0)
#define log_w(...) ((void)0)
constexpr int HTTPC_STRICT_FOLLOW_REDIRECTS=1;
class HTTPClient{
 public:
 NetworkClient* _client=nullptr;bool _reuse=true,_canReuse=false;
 std::unique_ptr<int> _tcpDeprecated,_transportTraits;
 ~HTTPClient();void end();void disconnect(bool);bool connected();NetworkClient* getStreamPtr();
 void clear(){};
 bool begin(NetworkClient&c,const String&){_client=&c;++opens;events.push_back("http-begin");return beginAllowed;}
 void useHTTP10(bool v){_reuse=!v;}void setFollowRedirects(int){}void setConnectTimeout(int){}
 void setTimeout(int){}void collectHeaders(const char**,size_t){}
 int GET(){events.push_back("http-get");transportConnected=responseCode>=200&&responseCode<300&&!streamMissing;return responseCode;}
 String header(const char*){return"";}
 int getSize(){return 0;}
};
// SDK_HTTP_METHODS
struct WiFiClass{void setSleep(bool){}}WiFi;
std::vector<uint8_t> mediaBytes;
struct File{
 bool good=false;size_t pos=0;explicit operator bool()const{return good;}void close(){if(good)events.push_back("file-close");good=false;}
 size_t read(uint8_t*out,size_t n){n=std::min(n,mediaBytes.size()-pos);std::memcpy(out,mediaBytes.data()+pos,n);pos+=n;return n;}
 size_t size(){return mediaBytes.size();}bool seek(uint32_t n){if(n>size())return false;pos=n;return true;}
};
constexpr int FILE_READ=0;
struct FS{File open(const char*,int){++opens;events.push_back("file-open");return{true};}}tab5FS;
// PRODUCTION_INPUT
// PRODUCTION_AVI_HELPERS
// PRODUCTION_GUARD
#define private public
#include "Tab5Video.h"
#undef private
void Tab5VideoEngine::setState(Tab5VideoState state,const String& status){_state.state=state;_state.status=status;}
void Tab5VideoEngine::logMemory(const char*)const{}
bool Tab5VideoEngine::playMjpeg(Tab5VideoInput&,float){events.push_back("play-mjpeg");return true;}
void Tab5VideoEngine::releaseDecoderResources(){events.push_back("decoder-release");}
void Tab5VideoEngine::releasePlayback(){events.push_back("playback-release");free(_jpegInput);_jpegInput=nullptr;}
void Tab5VideoEngine::releaseFrames(){}
bool Tab5VideoEngine::snapshot(Tab5VideoSnapshot& r)const{r=_state;return true;}
bool Tab5VideoEngine::ensureJpegInput(size_t n){free(_jpegInput);_jpegInput=static_cast<uint8_t*>(malloc(n));_jpegInputCapacity=n;return _jpegInput!=nullptr;}
bool Tab5VideoEngine::decodeAndPublish(const uint8_t*,size_t,uint64_t){return true;}
// PRODUCTION_AVI
// PRODUCTION_TASK
size_t where(const char*e){auto i=std::find(events.begin(),events.end(),e);assert(i!=events.end());return i-events.begin();}
void reset(){assert(clients==0&&leases==0);events.clear();stopDuringPriority=nullptr;streamMissing=false;audioMode=Tab5AudioMode::None;audioClaims=audioWrites=0;networkEnabled=true;priorityAllowed=true;responseCode=200;beginAllowed=true;}
using Bytes=std::vector<uint8_t>;
void u32(Bytes&out,uint32_t n){for(int i=0;i<4;++i)out.push_back(n>>(8*i));}
void tag(Bytes&out,const char*t){out.insert(out.end(),t,t+4);}
void chunk(Bytes&out,const char*t,const Bytes&data){tag(out,t);u32(out,data.size());out.insert(out.end(),data.begin(),data.end());if(data.size()&1)out.push_back(0);}
Bytes list(const char*t,const Bytes&data){Bytes b;tag(b,t);b.insert(b.end(),data.begin(),data.end());return b;}
Bytes makeAvi(){
 Bytes hdrl,video,audio,movi;
 Bytes vstrh(28);std::memcpy(vstrh.data(),"vidsMJPG",8);vstrh[20]=1;vstrh[24]=25;
 chunk(video,"strh",vstrh);
 Bytes astrh(28);std::memcpy(astrh.data(),"auds",4);chunk(audio,"strh",astrh);
 Bytes astrf(16);astrf[0]=1;astrf[2]=1;astrf[4]=0x80;astrf[5]=0x3e;astrf[14]=16;chunk(audio,"strf",astrf);
 chunk(hdrl,"LIST",list("strl",video));chunk(hdrl,"LIST",list("strl",audio));
 chunk(movi,"00dc",Bytes{0xff,0xd8,0xff,0xd9});chunk(movi,"01wb",Bytes{0,1,2,3});
 Bytes riff;tag(riff,"AVI ");chunk(riff,"LIST",list("hdrl",hdrl));chunk(riff,"LIST",list("movi",movi));
 Bytes result;chunk(result,"RIFF",riff);return result;
}
int main(){
 mediaBytes=makeAvi();
 std::atomic<bool> cancel{false};
 // EOF is the crucial path: SDK end() does not clear a disconnected client.
 {Tab5VideoInput in;Tab5AudioPriorityLease priority;
  assert(in.open("https://fixture.invalid/stream.mjpeg",&cancel INPUT_PRIORITY));
  transportConnected=false;in.close();in.close();
 }
 int cases=1;
 for(bool wifiOff:{false,true}){
  reset();networkEnabled=!wifiOff;cancel=!wifiOff;
  Tab5VideoInput in;Tab5AudioPriorityLease priority;
  assert(!in.open("http://fixture.invalid/cancel.mjpeg",&cancel INPUT_PRIORITY));
  in.close();assert(clients==0&&leases==0);++cases;
 }
 cancel=false;
 assert(clients==0&&leases==0);
 for(int repeat=0;repeat<100;++repeat){
  reset();Tab5VideoInput in;Tab5AudioPriorityLease priority;
  // HTTP rejection, transport already closed, immediate retry on same object.
  responseCode=503;assert(!in.open("http://fixture.invalid/a.mjpeg",&cancel INPUT_PRIORITY));
  responseCode=200;assert(in.open("http://fixture.invalid/b.mjpeg",&cancel INPUT_PRIORITY));
  in.close();assert(clients==0&&leases==0);cases+=2;
  // A malformed URL can leave HTTPClient borrowing a client before GET.
  beginAllowed=false;assert(!in.open("http://bad/a.mjpeg",&cancel INPUT_PRIORITY));
  beginAllowed=true;assert(clients==0&&leases==0);++cases;
  transportConnected=true;assert(in.open("http://fixture.invalid/live.mjpeg",&cancel INPUT_PRIORITY));
  in.close();assert(clients==0&&leases==0);++cases;
 }
 reset();streamMissing=true;
 {Tab5VideoInput in;Tab5AudioPriorityLease priority;
  assert(!in.open("http://fixture.invalid/empty.mjpeg",&cancel INPUT_PRIORITY));
  assert(clients==0&&leases==0);++cases;
 }
 // The entire worker method is the production method; only I/O and decoder
 // boundaries are controlled. Silent MJPEG, AVI and network all own priority.
 std::mutex mutex;
 for(const char*source:{"/Video/silent.mjpeg","/Video/pcm.avi","http://fixture.invalid/a.mjpeg"}){
  reset();Tab5VideoEngine e;e._mutex=&mutex;e._displayAttached=false;e._pendingSource=source;
  e.taskMain();
  const char*open=isRemoteSource(source)?"http-begin":"file-open";
  assert(where("priority-acquire")<where(open));
  assert(where("playback-release")<where("priority-release"));
  if(isRemoteSource(source))assert(where("client-delete")<where("priority-release"));
  else assert(where("file-close")<where("priority-release"));
  if(std::strstr(source,".avi"))assert(audioClaims==1&&audioWrites==1);
  assert(clients==0&&leases==0);++cases;
 }
 reset();priorityAllowed=false;Tab5VideoEngine denied;denied._mutex=&mutex;denied._pendingSource="/Video/x.mjpeg";
 const int before=opens;denied.taskMain();assert(opens==before);assert(denied._state.state==Tab5VideoState::Error);++cases;
 for(const char*source:{"/Video/recording.mjpeg","/Video/recording.avi"}){
  reset();priorityAllowed=false;audioMode=Tab5AudioMode::Recording;
  Tab5VideoEngine rec;rec._mutex=&mutex;rec._pendingSource=source;
  rec.taskMain();assert(rec._state.state==Tab5VideoState::Finished);
  assert(std::count(events.begin(),events.end(),"priority-acquire")==1);
  assert(std::count(events.begin(),events.end(),"priority-release")==0);
  assert(audioClaims==0&&audioWrites==0);
  assert(audioMode==Tab5AudioMode::Recording);++cases;
 }
 reset();Tab5VideoEngine stopped;stopped._mutex=&mutex;stopped._pendingSource="/Video/x.mjpeg";stopped._stopRequested=true;
 stopped.taskMain();assert(events.empty());++cases;
 reset();Tab5VideoEngine closeDuringAcquire;closeDuringAcquire._mutex=&mutex;closeDuringAcquire._pendingSource="/Video/x.mjpeg";
 stopDuringPriority=&closeDuringAcquire._stopRequested;
 const int openedBeforeCancel=opens;closeDuringAcquire.taskMain();assert(opens==openedBeforeCancel);
 assert(where("priority-acquire")<where("priority-release"));++cases;
 std::cout<<"{\"passed\":true,\"scenarios\":"<<cases<<",\"remainingTransports\":"<<clients<<",\"remainingLeases\":"<<leases<<",\"priorityBeforeEveryVideo\":true,\"cleanupBeforeAudioResume\":true}"<<std::endl;
}
