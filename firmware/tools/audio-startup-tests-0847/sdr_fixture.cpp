#include "Tab5Sdr.h"
#include "Tab5SdrDsp.h"
#include <atomic>
#include <new>
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks=0;static uint32_t clockMs=0;
static void check(bool ok,const char* msg){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",msg);exit(1);}}
uint32_t millis(){return clockMs;}void vTaskDelay(unsigned n){clockMs+=n;}void vTaskDelete(void*){}
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;static bool failAllocation=false;
static std::unordered_map<void*,size_t> allocated;
void* heap_caps_malloc(size_t size,unsigned){if(failAllocation)return nullptr;void* p=malloc(size);if(p)allocated[p]=size;return p;}
void heap_caps_free(void* p){if(p){check(allocated.erase(p)==1,"free exactly allocated SDR block");free(p);}}
namespace writing {bool usbHighSpeedActive(){return true;}}
struct Admission {bool ok=true;explicit operator bool()const{return ok;}};static bool failBudget=false,failTask=false;
namespace Tab5Resources {enum class Owner {Tools};struct Budget {size_t internal=0,internalBlock=0,psram=0,psramBlock=0;};Budget TaskBudget(size_t n){return {n+4096,n,0,0};}}
Admission tab5ReserveMemory(Tab5Resources::Owner,const Tab5Resources::Budget&,const char*){return {!failBudget};}
static void(*pendingTask)(void*)=nullptr;constexpr int pdPASS=1;
int tab5CreateAdmittedTask(Admission&,void(*fn)(void*),const char*,unsigned,void*,int,void*,int){if(failTask)return 0;pendingTask=fn;return pdPASS;}
void tab5TaskCheckpoint(const char*){}
enum class Tab5AudioClient {None,Sdr};struct Tab5PcmSpec {unsigned channels=0,sampleRate=0;};
struct AudioDevice {
 bool owned=false,available=true,failWrite=false;unsigned writes=0,claims=0,releases=0,releaseDeferrals=0;
 bool claim(Tab5AudioClient,unsigned){++claims;if(!available)return false;check(!owned,"no repeated successful claim");owned=true;return true;}
 Tab5AudioClient owner()const{return owned?Tab5AudioClient::Sdr:Tab5AudioClient::None;}
 void stop(Tab5AudioClient){}
 void release(Tab5AudioClient){check(owned,"audio released only when owned");if(releaseDeferrals){--releaseDeferrals;return;}owned=false;++releases;}
 bool writePcm(Tab5AudioClient,const int16_t*,size_t n,Tab5PcmSpec){check(owned&&n<=512,"audio bounded/owned");++writes;return !failWrite;}
} Tab5AudioDevice;
struct Audio {unsigned value=0;void setVolume(unsigned p){value=p;}} Tab5Audio;
struct rtlsdr_dev_t {uint32_t hz=0;unsigned direct=0;int ppm=0;};static rtlsdr_dev_t radio;
static unsigned opens=0,closes=0,reads=0,readLimit=30;static int failOpen=0,failRate=0;static uint32_t rejected=0;static bool failAllTune=false,failRead=false,shortRead=false;
static int(*cancelCallback)()=nullptr;static void(*stallCallback)()=nullptr;static std::function<void(unsigned)> onRead;
static std::vector<uint32_t> tuned;
void tab5_rtl_cancel_callback(int(*fn)()){cancelCallback=fn;}void tab5_rtl_stall_callback(void(*fn)()){stallCallback=fn;}
int rtlsdr_open(rtlsdr_dev_t** p,int){++opens;if(failOpen)return -1;radio={};*p=&radio;return 0;}void rtlsdr_close(rtlsdr_dev_t*){++closes;}
int rtlsdr_set_sample_rate(rtlsdr_dev_t*,unsigned){return failRate?-1:0;}
int tab5_rtlsdr_set_input_and_freq(rtlsdr_dev_t* p,int d,uint32_t f){tuned.push_back(f);if(failAllTune||f==rejected)return -1;p->hz=f;p->direct=d;return 0;}
int rtlsdr_get_freq_correction(rtlsdr_dev_t* d){return d->ppm;}int rtlsdr_set_freq_correction(rtlsdr_dev_t* d,int v){d->ppm=v;return v==9?-1:0;}
int rtlsdr_reset_buffer(rtlsdr_dev_t*){return 0;}int rtlsdr_set_tuner_gain_mode(rtlsdr_dev_t*,int){return 0;}int rtlsdr_set_agc_mode(rtlsdr_dev_t*,int){return 0;}
int rtlsdr_read_sync(rtlsdr_dev_t*,void* data,int length,int* bytes){
 ++reads;clockMs+=20;if(onRead)onRead(reads);if(reads>=readLimit)Tab5Sdr::stop();if(failRead)return -1;
 auto* iq=static_cast<uint8_t*>(data);for(int i=0;i<length/2;++i){double t=6.28318530718*6000*i/240000;iq[i*2]=uint8_t(127.5+70*cos(t));iq[i*2+1]=uint8_t(127.5+70*sin(t));}*bytes=shortRead?1024:length;return 0;
}
/* PRODUCTION */
static void reset(){check(!Tab5Sdr::busy()&&allocated.empty(),"previous worker fully released");clockMs=100;opens=closes=reads=0;readLimit=30;failOpen=failRate=0;failBudget=failTask=failAllocation=failRead=failAllTune=shortRead=false;rejected=0;onRead=nullptr;tuned.clear();Tab5AudioDevice=AudioDevice{};pendingTask=nullptr;}
static void execute(){check(pendingTask!=nullptr,"worker created");auto fn=pendingTask;pendingTask=nullptr;fn(nullptr);check(!Tab5Sdr::busy()&&allocated.empty()&&!Tab5AudioDevice.owned,"stop releases PSRAM and audio");check(!cancelCallback&&!stallCallback,"USB callbacks cleared after completion");}
static void models(){
 uint32_t hz=0;check(Tab5Sdr::frequency("531",1000,hz)&&hz==531000,"AM kHz exact");check(Tab5Sdr::frequency("100.125",1000000,hz)&&hz==100125000,"fractional MHz exact");for(const char* bad:{"","nan","inf","-1","0","1.2.3","1e4","4294967296"})check(!Tab5Sdr::frequency(bad,1,hz),"invalid frequency rejected");check(Tab5Sdr::frequency("4294967295",1,hz)&&hz==UINT32_MAX,"unsigned frequency boundary accepted");
 Tab5Sdr::Scan scan;scan.first=UINT32_MAX-10;scan.last=UINT32_MAX;scan.step=7;check(Tab5Sdr::valid(scan),"high scan valid");check(Tab5Sdr::nextFrequency(scan,scan.first,hz)&&hz==UINT32_MAX-3,"scan no overflow");check(!Tab5Sdr::nextFrequency(scan,hz,hz),"scan upper bound exact");scan.first=1;scan.last=100000;scan.step=1;check(!Tab5Sdr::valid(scan),"unbounded scan rejected");
 Tab5Sdr::DetectionList records;for(unsigned i=0;i<Tab5Sdr::MaxDetections;++i)check(records.add({100000+i,2,1,-40}),"record added");check(!records.add({999999,2,1,-40})&&records.full,"list bounded");check(records.add({100000,2,1,-20})&&records.count==Tab5Sdr::MaxDetections&&records.values[0].db==-20,"duplicate merged strongest power");char csv[Tab5Sdr::MaxDetections*48+16];size_t n=records.serialize(csv,sizeof(csv));check(n>0,"records serialize");Tab5Sdr::DetectionList loaded;check(loaded.parse(csv,n)&&loaded.count==records.count,"CSV round trip");check(!loaded.parse(csv,n-1),"truncation rejected");check(!loaded.parse("TAB5-SDR,1\n531000,2,1,-140\n",26),"invalid dB rejected");records.erase(0);check(records.count==Tab5Sdr::MaxDetections-1&&!records.full&&records.values[0].hz==100001,"delete frees capacity");
 Tab5SdrDsp::Dsp dsp;uint8_t iq[4096];int8_t bins[256];for(unsigned i=0;i<2048;++i){double phase=6.28318530718*32*i/1024;iq[2*i]=uint8_t(127.5+63.75*cos(phase));iq[2*i+1]=uint8_t(127.5+63.75*sin(phase));}dsp.spectrum(iq,bins);auto peak=std::max_element(bins,bins+256);check(peak-bins==136&&*peak>=-7&&*peak<=-5,"FFT frequency and Hann-normalized -6 dBFS tone");
 int16_t out[514];out[512]=12345;out[513]=-12345;for(unsigned mode:{1u,2u,3u})for(unsigned cycle=0;cycle<100;++cycle){size_t count=dsp.audio(iq,sizeof(iq),mode,out);check(count<=410&&out[512]==12345&&out[513]==-12345,"audio output guard untouched");}dsp.resetAudio();check(dsp.decimation==0&&dsp.prevI==0,"retune resets demodulator memory");
 // A real centred AM carrier must retain the 1 kHz modulation rather than
 // turn into the 2 kHz rectified waveform caused by subtracting the carrier.
 double firstReal=0,firstImag=0,secondReal=0,secondImag=0;uint64_t sample=0,outputSample=0;
 for(unsigned block=0;block<350;++block){for(unsigned i=0;i<2048;++i,++sample){double phase=6.28318530718*1000*double(sample)/240000.;iq[2*i]=uint8_t(127.5+55+25*sin(phase));iq[2*i+1]=128;}
  size_t n=dsp.audio(iq,sizeof(iq),2,out);for(size_t k=0;k<n;++k,++outputSample)if(block>180){double phase=6.28318530718*1000*double(outputSample)/48000.;firstReal+=out[k]*cos(phase);firstImag+=out[k]*sin(phase);secondReal+=out[k]*cos(2*phase);secondImag+=out[k]*sin(2*phase);}}
 check(hypot(firstReal,firstImag)>10*hypot(secondReal,secondImag),"AM centred carrier preserves original 1 kHz modulation");
}
int main(){models();
 reset();failBudget=true;check(!Tab5Sdr::start({531000,2,1})&&allocated.empty(),"admission failure no allocation");reset();failAllocation=true;check(!Tab5Sdr::start({531000,2,1})&&allocated.empty(),"PSRAM OOM no worker");reset();failTask=true;check(!Tab5Sdr::start({531000,2,1})&&allocated.empty(),"task OOM releases state");
 reset();failOpen=1;check(Tab5Sdr::start({531000,2,1}),"start queues despite absent key");execute();check(opens==1&&closes==0,"open failure has no spurious close");
 reset();failRate=1;Tab5Sdr::start({100000000,1,0});execute();check(closes==1,"sample-rate failure closes USB");
 reset();rejected=531000;Tab5Sdr::start({531000,2,0});execute();check(closes==1&&reads==0,"tuner rejection closes without reading");
 reset();Tab5Sdr::start({100000000,1,0});onRead=[](unsigned n){if(n==5)check(Tab5Sdr::configure({100100000,3,0}),"live retune queued");if(n==10)check(Tab5Sdr::configure({531000,2,1}),"live input/mode change queued");};execute();check(opens==1&&closes==1&&tuned.size()==3&&Tab5Sdr::current().hz==531000,"settings use one open USB session");
 reset();Tab5Sdr::start({100000000,1,0});onRead=[](unsigned n){if(n==5)check(Tab5Sdr::configure({100000000,1,0,-18,75}),"live ppm/deemphasis queued");};execute();check(Tab5Sdr::current().ppm==-18&&Tab5Sdr::current().deemphasis==75&&radio.ppm==-18,"ppm and de-emphasis applied without reopening USB");
 reset();Tab5Sdr::start({100000000,1,0,-18,75});onRead=[](unsigned n){if(n==5)Tab5Sdr::configure({100000000,1,0,9,50});};execute();check(Tab5Sdr::current().ppm==-18&&Tab5Sdr::current().deemphasis==75&&radio.ppm==-18,"failed ppm change restores actual and displayed calibration");
 reset();rejected=200000000;Tab5Sdr::start({100000000,1,0});onRead=[](unsigned n){if(n==5)Tab5Sdr::configure({200000000,2,0});};execute();check(Tab5Sdr::current().hz==100000000&&tuned.back()==100000000,"rejected retune restores original frequency");
 reset();Tab5Sdr::Scan sweep;sweep.first=100000000;sweep.last=100200000;sweep.step=100000;sweep.dwell=200;sweep.threshold=-25;readLimit=55;Tab5Sdr::scan(sweep,{sweep.first,1,0});execute();check(tuned.back()==sweep.last&&!Tab5Sdr::scanning(),"scan stops at final channel");Tab5Sdr::Detection found;unsigned detections=0;while(Tab5Sdr::takeDetection(found))++detections;check(detections==3,"stable signals recorded for three channels");
 reset();sweep.hold=true;readLimit=70;bool resumed=false;Tab5Sdr::scan(sweep,{sweep.first,1,0});onRead=[&](unsigned){if(Tab5Sdr::scanPaused()&&!resumed){resumed=true;Tab5Sdr::pauseScan(false);}};execute();check(resumed&&tuned.back()==100100000,"resume moves past held signal");
 reset();Tab5AudioDevice.available=false;Tab5Sdr::start({100000000,1,0});execute();check(Tab5AudioDevice.claims<3,"busy audio owner is not polled every USB read");
 reset();shortRead=true;Tab5Sdr::start({100000000,1,0});execute();check(Tab5AudioDevice.writes>0,"short USB blocks still feed audio without FFT overread");
 reset();failRead=true;Tab5Sdr::start({100000000,1,0});execute();check(reads==1&&closes==1,"USB failure closes session");
 reset();Tab5AudioDevice.failWrite=true;Tab5Sdr::start({100000000,1,0});execute();
 const unsigned failedOutputWrites=Tab5AudioDevice.writes;
 check(PATCHED?failedOutputWrites==1:failedOutputWrites>1,"terminal audio failure stops retries, preserves SDR reception");check(reads==readLimit&&closes==1,"spectrum worker continues until explicit stop");
 reset();Tab5AudioDevice.failWrite=true;Tab5Sdr::start({100000000,1,0});
 onRead=[](unsigned n){if(n==10){Tab5AudioDevice.failWrite=false;check(Tab5Sdr::configure({100100000,1,0}),"explicit command can retry output");}};
 execute();check(Tab5AudioDevice.writes>1,"output works after user retry");check(Tab5AudioDevice.claims==(PATCHED?2u:1u),"no automatic hardware retry before command");
 #if PATCHED
 reset();Tab5AudioDevice.failWrite=true;Tab5AudioDevice.releaseDeferrals=3;readLimit=5;Tab5Sdr::start({100000000,1,0});execute();
 check(Tab5AudioDevice.releases==1&&Tab5AudioDevice.releaseDeferrals==0,"deferred ownership release completed before worker retirement");
 #endif
 for(unsigned cycle=0;cycle<2000;++cycle){reset();readLimit=5;Tab5Sdr::start({100000000,1,0});execute();}
 Tab5Sdr::volume(99);check(Tab5Audio.value==252,"SDR volume goes through persistent global setter");printf("PASS %u SDR model/DSP/worker checks; 2000 lifecycle cycles; driver and RTOS boundaries simulated\n",checks);
}
