#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include "Tab5AudioPriority.h"
#include "Tab5ConnectionPolicy.h"
using String=std::string;using TickType_t=unsigned;
constexpr unsigned portMAX_DELAY=~0U;
uint32_t millis(){return 500;}bool audioCommandPending(){return false;}
constexpr int WL_CONNECTED=3;struct{int status(){return WL_CONNECTED;}}WiFi;
#define TAB5_LOG(...) ((void)0)
struct{bool fault=false;bool failed(){return fault;}}Tab5AudioDevice;
enum class Tab5AudioMode{Idle,Local,Radio,Recording};
struct Radio{bool receiving(){return true;}};
class Tab5AudioEngine{
 public:
 class StateGuard;
 bool lockState(TickType_t)const{return true;}void unlockState()const{}
 Tab5AudioPriorityControl _memoryPriority;
 Tab5AudioMode _mode=Tab5AudioMode::Local;
 bool _paused=false,_playingRecording=false,_wifiEnabled=true,_radioReconnectPending=false,_wifiPending=false,_playRadioAfterWifi=false;
 unsigned _radioRetryCount=0,_playStartedAt=0,_radioReconnectAt=0,_stationIndex=0;
 String _nowPlaying="sample",_wifiSsid,_wifiPassword,_status;
 Radio radio;Radio*_radioSource=nullptr;
 bool decodeResult=true,running=true;unsigned stops=0,loops=0,nexts=0,reconnects=0,cleanups=0;
 void serviceWifiScan(){}void serviceWifiPower(){}void serviceTelemetry(){}void serviceRecording(){}void serviceWifiConnection(){}
 bool decoderRunning(){return running;}bool decoderLoop(){++loops;return decodeResult;}
 void cleanupDecoder(){++cleanups;_radioSource=nullptr;running=false;}
 void stopPlayback(bool){++stops;cleanupDecoder();_mode=Tab5AudioMode::Idle;_radioReconnectPending=false;}
 void setStatus(const String&s){_status=s;}void setNowPlaying(const String&s){_nowPlaying=s;}
 bool selectAdjacentTrack(int,bool){++nexts;return false;}
 bool startRadioNow(unsigned){++reconnects;return true;}bool execConnectWifi(const String&,const String&,bool){++reconnects;return true;}
 void serviceDecoder();
};
// PRODUCTION_METHODS
int main(){unsigned checks=0;
 for(bool radio:{false,true})for(bool decoded:{false,true}){
  Tab5AudioEngine e;e._mode=radio?Tab5AudioMode::Radio:Tab5AudioMode::Local;e._radioSource=radio?&e.radio:nullptr;e.decodeResult=decoded;Tab5AudioDevice.fault=true;
  e.serviceDecoder();assert(e.loops==1);++checks;
  if(PATCHED){assert(e.stops==1&&e.nexts==0&&!e._radioReconnectPending&&e.reconnects==0);assert(e._status.find("Sortie audio indisponible")!=String::npos);checks+=2;}
  else{assert(e.stops==0);assert(e.nexts==unsigned(!radio&&!decoded));assert(e._radioReconnectPending==(radio&&!decoded));checks+=3;}
 }
 // Healthy EOF preserves both local completion and radio reconnect semantics.
 Tab5AudioDevice.fault=false;
 for(bool radio:{false,true}){
  Tab5AudioEngine e;e._mode=radio?Tab5AudioMode::Radio:Tab5AudioMode::Local;e._radioSource=radio?&e.radio:nullptr;e.decodeResult=false;
  e.serviceDecoder();assert(e.stops==0&&e.loops==1);assert(e.nexts==unsigned(!radio));assert(e._radioReconnectPending==radio);checks+=3;
 }
 for(bool pause:{false,true}){
  Tab5AudioEngine e;if(pause)e._paused=true;else{auto token=e._memoryPriority.request();assert(token);assert(e._memoryPriority.begin()==token);e._memoryPriority.acknowledge(token,true);}
  e.serviceDecoder();assert(e.loops==0&&e.stops==0&&e.nexts==0);++checks;
 }
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"faultCases\":4,\"healthyEofCases\":2,\"heldOrPausedCases\":2}"<<std::endl;
}
