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
#include <deque>
#include <functional>
#include "Tab5ConnectionPolicy.h"
#include "Tab5CatalogueRevision.h"
// COMMAND_TYPES
using TickType_t=unsigned;
constexpr unsigned portMAX_DELAY=~0U,pdTRUE=1;
static thread_local unsigned stateDepth=0;
static unsigned unprotectedReads=0,slowCalls=0,checks=0;
static void outsideState(){assert(stateDepth==0);++slowCalls;}
struct String{std::string s;String()=default;String(const char*x):s(x?x:""){}String(std::string x):s(std::move(x)){}const char*c_str()const{return s.c_str();}size_t length()const{return s.size();}void toLowerCase(){for(auto&c:s)c=std::tolower((unsigned char)c);}bool startsWith(const char*x)const{return s.rfind(x,0)==0;}bool endsWith(const char*x)const{size_t n=std::strlen(x);return s.size()>=n&&s.compare(s.size()-n,n,x)==0;}int indexOf(const char*x)const{auto p=s.find(x);return p==s.npos?-1:int(p);}friend bool operator==(const String&a,const String&b){return a.s==b.s;}friend String operator+(const String&a,const String&b){return a.s+b.s;}};
struct GuardedVolume{uint8_t v=150;operator uint8_t()const{if(!stateDepth)++unprotectedReads;return v;}GuardedVolume&operator=(uint8_t n){assert(stateDepth);v=n;return *this;}};
uint32_t millis(){return 1234;}unsigned pdMS_TO_TICKS(unsigned n){return n;}
static unsigned opened=0;static String lastOpened;
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
struct AudioFileSource{virtual~AudioFileSource()=default;virtual bool open(const char*name){outsideState();++opened;lastOpened=name;return true;}virtual bool isOpen(){return true;}};
struct AudioFileSourceSD:AudioFileSource{};
struct Tab5BufferedRadioSource:AudioFileSource{size_t capacityBytes(){return 65536;}};
struct Decoder{Decoder()=default;Decoder(void*,size_t){}virtual~Decoder()=default;bool begin(AudioFileSource*,Output*){outsideState();return true;}};
using AudioGeneratorMP3=Decoder;using Tab5SeekableMp3Generator=Decoder;using Tab5SeekableAacGenerator=Decoder;using Tab5SeekableFlacGenerator=Decoder;
struct Tab5SeekableWavGenerator:Decoder{uint32_t durationSeconds(){return 12;}};
namespace Tab5MediaSession{void volume(uint8_t,uint32_t){}}
int xSemaphoreTake(void*,unsigned){return pdTRUE;}void xSemaphoreGive(void*){}
enum class Tab5AudioMode{Idle,Local,Radio};struct Tab5AudioStation{String name,style,url;};
static std::deque<Tab5AudioCommand> commands;
int xQueueReceive(void*,Tab5AudioCommand*c,unsigned){if(commands.empty())return 0;*c=commands.front();commands.pop_front();return pdTRUE;}
class Tab5AudioEngine{
public:
 class StateGuard;
 mutable std::recursive_mutex state;
 mutable std::function<void()> onUnlock;
 bool lockState(TickType_t)const{state.lock();++stateDepth;return true;}void unlockState()const{assert(stateDepth);--stateDepth;state.unlock();if(!stateDepth&&onUnlock){auto cb=std::move(onUnlock);onUnlock=nullptr;cb();}}
 GuardedVolume _volume;Output*_output;std::vector<Tab5AudioStation>_stations;std::vector<String>_tracks;
 std::atomic<uint32_t> _stationCatalogueRevision{2},_trackCatalogueRevision{2};
 uint32_t trackCatalogueRevision()const{return tab5CatalogueRevision(_trackCatalogueRevision);}
 uint32_t stationCatalogueRevision()const{return tab5CatalogueRevision(_stationCatalogueRevision);}
 size_t _trackIndex=0;String _selectedRadioUrl,_wifiSsid="test",_wifiPassword,_status;
 bool _wifiEnabled=true,_wifiPending=false,_playRadioAfterWifi=false;unsigned _radioRetryCount=0;
 std::function<void()> onPublish;
 void*_commandQueue=(void*)1;
 bool deferPriorityCommand(const Tab5AudioCommand&){return false;}
 void handleCommand(const Tab5AudioCommand&);void drainCommands();
 bool execPlayPath(const String&path){return startLocalDecoder(path);}void execSeekPermille(unsigned){}void execNext(){}void execPrevious(){}void execTogglePause(){}void execStop(){}void execStartRecording(){}void execStopRecording(){}void execStartWifiScan(){}void execSetWifiEnabled(bool){}void execForgetWifi(){}
 bool execConnectWifi(const String&,const String&,bool){return true;}bool loadWifiCredentials(){return true;}
 void publishSnapshot(){if(onPublish)onPublish();}
#if PATCHED
 bool execPlayTrack(size_t,uint32_t=0);bool execPlayStation(size_t,uint32_t=0);bool startRadioNow(size_t,uint32_t=0);bool resumeSelectedStation();
#else
 bool execPlayTrack(size_t);bool execPlayStation(size_t);bool startRadioNow(size_t);bool resumeSelectedStation(){return execPlayStation(_stationIndex);}
#endif
 bool _sdAvailable=true,_radioStartCancelled=false,_paused=false,_playingRecording=false,_radioReconnectPending=false,_uiDirty=false;
 uint32_t _localEstimatedSeconds=0,_playStartedAt=0,_pauseStartedAt=0,_radioReconnectAt=0;
 String _currentLocalPath,_currentAlbumFolder,_stationCategory;size_t _stationIndex=0;
 Tab5AudioMode _mode=Tab5AudioMode::Idle;
 AudioFileSourceSD*_localSource=nullptr;AudioFileSource*_streamSource=nullptr;Tab5BufferedRadioSource*_radioSource=nullptr;
 Decoder*_mp3=nullptr,*_aac=nullptr,*_flac=nullptr;Tab5SeekableWavGenerator*_wav=nullptr;
 void*_mp3CodecBuffer=nullptr,*_aacCodecBuffer=nullptr;size_t _mp3CodecBufferSize=1024;
 Tab5NetworkLease*_radioLease=nullptr;void*_snapshotMutex=(void*)1;struct{uint8_t volume=0;}_snapshot;
 bool startLocalDecoder(const String&);void setVolume(uint8_t);bool selectAdjacentTrack(int,bool);bool selectAdjacentStation(int);
 void setStatus(const String&s){_status=s;}void setNowPlaying(const String&){}
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
 assert(unprotectedReads==0);claimOk=true;
 e._tracks={"/Audio/one.mp3","/Audio/two.mp3"};
 Tab5AudioCommand cmd;cmd.type=Tab5AudioCommandType::PlayTrack;cmd.index=0;cmd.catalogueRevision=e.trackCatalogueRevision();commands.push_back(cmd);
 {Tab5CatalogueChange change(e._trackCatalogueRevision);std::swap(e._tracks[0],e._tracks[1]);}
 unsigned before=opened;e.drainCommands();assert(opened-before==(PATCHED?0u:1u));++checks;
 cmd.catalogueRevision=e.trackCatalogueRevision();commands.push_back(cmd);e.drainCommands();assert(lastOpened=="/Audio/two.mp3");++checks;
 cmd.type=Tab5AudioCommandType::PlayStation;cmd.catalogueRevision=e.stationCatalogueRevision();commands.push_back(cmd);
 {Tab5CatalogueChange change(e._stationCatalogueRevision);std::swap(e._stations[0],e._stations[1]);}
 before=opened;e.drainCommands();assert(opened-before==(PATCHED?0u:1u));++checks;
 cmd.catalogueRevision=e.stationCatalogueRevision();commands.push_back(cmd);e.drainCommands();assert(lastOpened=="https://radio/aac.aac");++checks;
 // Reorder after initial snapshot, before connected startup's second lookup.
 cmd.catalogueRevision=e.stationCatalogueRevision();commands.push_back(cmd);
 e.onPublish=[&]{Tab5CatalogueChange change(e._stationCatalogueRevision);std::swap(e._stations[0],e._stations[1]);};
 before=opened;e.drainCommands();e.onPublish=nullptr;assert(opened-before==(PATCHED?0u:1u));++checks;
 // Resume uses the selected URL rather than a stale numeric index.
 e._selectedRadioUrl="https://radio/aac.aac";e._stationIndex=0;
 assert(e.resumeSelectedStation());assert((lastOpened=="https://radio/aac.aac")==bool(PATCHED));++checks;
 {Tab5CatalogueChange change(e._stationCatalogueRevision);e._stations.resize(1);}
 e._selectedRadioUrl="https://radio/aac.aac";e._stationIndex=0;
 before=opened;bool resumed=e.resumeSelectedStation();assert(resumed==!PATCHED);assert(opened-before==(PATCHED?0u:1u));checks+=2;

 // The current source remains authoritative if catalogue indices move.
 e._tracks={"/Audio/a.mp3","/Audio/b.mp3","/Audio/c.mp3"};
 e._currentLocalPath="/Audio/b.mp3";e._trackIndex=0;e._currentAlbumFolder="/Audio";
 assert(e.selectAdjacentTrack(1,false));assert(lastOpened==(PATCHED?"/Audio/c.mp3":"/Audio/b.mp3"));checks+=2;
 e._currentLocalPath="/Audio/removed.mp3";e._trackIndex=0;
 before=opened;assert(e.selectAdjacentTrack(1,false)==!PATCHED);assert(opened-before==(PATCHED?0u:1u));checks+=2;
 e._stations={{"a","test","https://radio/a"},{"b","test","https://radio/b"},{"c","test","https://radio/c"}};
 e._selectedRadioUrl="https://radio/b";e._stationIndex=0;e._stationCategory="test";
 assert(e.selectAdjacentStation(1));assert(lastOpened==(PATCHED?"https://radio/c":"https://radio/b"));checks+=2;
 e._selectedRadioUrl="https://radio/removed";e._stationIndex=0;
 before=opened;assert(e.selectAdjacentStation(1)==!PATCHED);assert(opened-before==(PATCHED?0u:1u));checks+=2;
 // A stale index beyond the new size must not reject a surviving URL.
 e._selectedRadioUrl="https://radio/a";e._stationIndex=10;
 assert(e.selectAdjacentStation(1)==bool(PATCHED));if(PATCHED)assert(lastOpened=="https://radio/b");++checks;
 // Change after neighbour selection but before the selected index is executed.
 e._selectedRadioUrl="https://radio/a";e._stationIndex=0;
 e.onUnlock=[&]{Tab5CatalogueChange change(e._stationCatalogueRevision);std::swap(e._stations[1],e._stations[2]);};
 before=opened;assert(e.selectAdjacentStation(1)==!PATCHED);assert(opened-before==(PATCHED?0u:1u));checks+=2;

 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"runtimeVolumeReadsOutsideGuard\":"<<unprotectedReads<<",\"slowCallsOutsideGuard\":"<<slowCalls<<",\"startCycles\":301}"<<std::endl;
}
