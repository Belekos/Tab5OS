#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <new>
#include <string>
#include <vector>
using TickType_t=unsigned;
constexpr unsigned portMAX_DELAY=~0U,pdTRUE=1;
static thread_local unsigned stateDepth=0;
static unsigned unprotectedReads=0,slowCalls=0,checks=0;
static void outsideState(){assert(stateDepth==0);++slowCalls;}
struct String{std::string s;String()=default;String(const char*x):s(x?x:""){}String(std::string x):s(std::move(x)){}const char*c_str()const{return s.c_str();}size_t length()const{return s.size();}void toLowerCase(){for(auto&c:s)c=std::tolower((unsigned char)c);}bool startsWith(const char*x)const{return s.rfind(x,0)==0;}bool endsWith(const char*x)const{size_t n=std::strlen(x);return s.size()>=n&&s.compare(s.size()-n,n,x)==0;}int indexOf(const char*x)const{auto p=s.find(x);return p==s.npos?-1:int(p);}friend String operator+(const String&a,const String&b){return a.s+b.s;}};
struct GuardedVolume{uint8_t v=150;operator uint8_t()const{if(!stateDepth)++unprotectedReads;return v;}GuardedVolume&operator=(uint8_t n){assert(stateDepth);v=n;return *this;}};
uint32_t millis(){return 1234;}
constexpr unsigned MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2;
size_t heap_caps_get_free_size(unsigned){return 100000;}
unsigned uxTaskGetStackHighWaterMark(void*){return 8192;}
struct Serial{template<class...T>void printf(const char*,T...){}void flush(){}}Tab5DebugSerial;
#define TAB5_LOG(...) ((void)0)
void tab5LogHeap(const char*){}
struct{bool exists(const char*){outsideState();return true;}}tab5FS;
static bool isAudioPath(const String&){return true;}
static uint32_t estimateLocalDuration(const String&){outsideState();return 10;}
static bool isRecordingPath(const String&){return false;}
static String parentFolder(const String&){return "/Audio";}
static String baseName(const String&s){return s;}
static size_t aacWorkspaceBytes(){return 4096;}
static bool network=true,command=false,pending=false;
bool tab5NetworkEnabled(){return network;}bool audioCommandPending(){return command;}
void tab5NetworkSetMediaPending(bool p){pending=p;}
struct Tab5NetworkLease{Tab5NetworkLease(const char*,unsigned,bool(*)(void*)){outsideState();}explicit operator bool()const{return true;}void shareAfterSetup(){outsideState();}};
namespace Tab5Resources{enum class Owner{Audio};struct AdmissionReport{};struct Budget{size_t internal=40960;};struct Reservation{explicit operator bool()const{return true;}void allocated(size_t,size_t){}void release(){}};Budget radioBudget(bool){return{};}void describeAdmission(const AdmissionReport&,char*p,size_t n){if(n)*p=0;}}
Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Budget,const char*,Tab5Resources::AdmissionReport*){outsideState();return{};}
struct{struct SpeakerType{struct Config{unsigned dma_buf_len=256,dma_buf_count=8;};Config config(){return{};}}Speaker;}M5;
enum class Tab5AudioClient{Audio};
static bool claimOk=true;
struct Device{uint8_t actual=0;void setVolume(uint8_t v){actual=v;}bool claim(Tab5AudioClient,unsigned){outsideState();return claimOk;}}Tab5AudioDevice;
constexpr int WL_CONNECTED=3;struct{int status(){return WL_CONNECTED;}void setSleep(bool){outsideState();}}WiFi;
enum class Tab5RadioNetworkStage{HttpOpen,DecoderStart,Playing};void tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage){}void tab5NetDiagnosticsService(){}
struct Output{void setMasterVolume(uint8_t v){Tab5AudioDevice.setVolume(v);}};
struct AudioFileSource{virtual~AudioFileSource()=default;virtual bool open(const char*){outsideState();return true;}virtual bool isOpen(){return true;}};
struct AudioFileSourceSD:AudioFileSource{};
struct Tab5BufferedRadioSource:AudioFileSource{size_t capacityBytes(){return 65536;}};
struct Decoder{Decoder()=default;Decoder(void*,size_t){}virtual~Decoder()=default;bool begin(AudioFileSource*,Output*){outsideState();return true;}};
using AudioGeneratorMP3=Decoder;using Tab5SeekableMp3Generator=Decoder;using Tab5SeekableAacGenerator=Decoder;using Tab5SeekableFlacGenerator=Decoder;
struct Tab5SeekableWavGenerator:Decoder{uint32_t durationSeconds(){return 12;}};
namespace Tab5MediaSession{void volume(uint8_t,uint32_t){}}
int xSemaphoreTake(void*,unsigned){return pdTRUE;}void xSemaphoreGive(void*){}
enum class Tab5AudioMode{Idle,Local,Radio};struct Tab5AudioStation{String name,style,url;};
class Tab5AudioEngine{
public:
 class StateGuard;
 mutable std::recursive_mutex state;
 bool lockState(TickType_t)const{state.lock();++stateDepth;return true;}void unlockState()const{assert(stateDepth);--stateDepth;state.unlock();}
 GuardedVolume _volume;Output*_output;std::vector<Tab5AudioStation>_stations;
 bool _sdAvailable=true,_radioStartCancelled=false,_paused=false,_playingRecording=false,_radioReconnectPending=false,_uiDirty=false;
 uint32_t _localEstimatedSeconds=0,_playStartedAt=0,_pauseStartedAt=0,_radioReconnectAt=0;
 String _currentLocalPath,_currentAlbumFolder,_stationCategory;size_t _stationIndex=0;
 Tab5AudioMode _mode=Tab5AudioMode::Idle;
 AudioFileSourceSD*_localSource=nullptr;AudioFileSource*_streamSource=nullptr;Tab5BufferedRadioSource*_radioSource=nullptr;
 Decoder*_mp3=nullptr,*_aac=nullptr,*_flac=nullptr;Tab5SeekableWavGenerator*_wav=nullptr;
 void*_mp3CodecBuffer=nullptr,*_aacCodecBuffer=nullptr;size_t _mp3CodecBufferSize=1024;
 Tab5NetworkLease*_radioLease=nullptr;void*_snapshotMutex=(void*)1;struct{uint8_t volume=0;}_snapshot;
 bool startLocalDecoder(const String&);bool startRadioNow(size_t);void setVolume(uint8_t);
 void setStatus(const String&){}void setNowPlaying(const String&){}
 bool ensureMp3CodecBuffer(){outsideState();return true;}bool ensureAacCodecBuffer(){outsideState();return true;}
 void cleanupDecoder(){outsideState();delete _localSource;_localSource=nullptr;delete _streamSource;_streamSource=nullptr;_radioSource=nullptr;delete _mp3;_mp3=nullptr;delete _aac;_aac=nullptr;delete _flac;_flac=nullptr;delete _wav;_wav=nullptr;delete _radioLease;_radioLease=nullptr;pending=false;}
 void stopPlayback(bool){cleanupDecoder();}
 ~Tab5AudioEngine(){cleanupDecoder();}
};
// PRODUCTION_METHODS
int main(){
 Output output;Tab5AudioEngine e;e._output=&output;e._stations={{"mp3","test","https://radio/mp3"},{"aac","test","https://radio/aac.aac"}};
 for(unsigned cycle=0;cycle<50;++cycle){
  const uint8_t desired=uint8_t(40+cycle);e.setVolume(desired);
  for(const char*path:{"/Audio/test.mp3","/Audio/test.aac","/Audio/test.wav","/Audio/test.flac"}){Tab5AudioDevice.actual=19;assert(e.startLocalDecoder(path));assert(Tab5AudioDevice.actual==desired);assert(!stateDepth);checks+=3;}
  for(size_t station:{0U,1U}){Tab5AudioDevice.actual=19;assert(e.startRadioNow(station));assert(Tab5AudioDevice.actual==desired);assert(!stateDepth);checks+=3;}
 }
 claimOk=false;assert(!e.startRadioNow(0));assert(!stateDepth&&!e._radioLease);checks+=2;
 assert(unprotectedReads==EXPECTED_UNPROTECTED_READS);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"runtimeVolumeReadsOutsideGuard\":"<<unprotectedReads<<",\"slowCallsOutsideGuard\":"<<slowCalls<<",\"startCycles\":301}"<<std::endl;
}
