#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <vector>
using BaseType_t=int;using SemaphoreHandle_t=std::mutex*;constexpr int pdTRUE=1;constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
uint32_t clockMs=0;uint32_t millis(){return clockMs;}uint32_t pdMS_TO_TICKS(uint32_t n){return n;}void vTaskDelay(uint32_t n){clockMs+=n;}
std::mutex owner;SemaphoreHandle_t xSemaphoreCreateMutex(){return &owner;}int xSemaphoreTake(SemaphoreHandle_t m,unsigned){return m->try_lock()?pdTRUE:0;}void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
unsigned allocated=0;void*heap_caps_calloc(size_t n,size_t b,unsigned){auto p=std::calloc(n,b);if(p)++allocated;return p;}void heap_caps_free(void*p){if(p)--allocated;std::free(p);}
struct Debug{template<class...A>void printf(const char*,A...){}void flush(){}}Tab5DebugSerial;
class AudioOutput{public:virtual~AudioOutput()=default;virtual bool begin(){return true;}virtual bool ConsumeSample(int16_t*){return true;}virtual bool SetRate(int){return true;}virtual bool SetChannels(int){return true;}virtual void flush(){}virtual bool stop(){return true;}int hertz=0;uint8_t channels=0,gainF2P6=0;};
#define CONFIG_FREERTOS_UNICORE 1
#define SDL_h_ 1
void SDL_Delay(unsigned n){clockMs+=n;}
constexpr unsigned SAMPLERATE_MUL=256;
struct FakeSpeaker{
 struct Config{uint32_t sample_rate=48000;unsigned dma_buf_len=256,dma_buf_count=8,task_priority=0,task_pinned_core=0;} _cfg;
 struct wav_info_t{const void*data=nullptr;size_t length=0;uint32_t repeat=0;float sample_rate_x256=0;bool is_stereo=false,is_16bit=false,is_signed=false,stop_current=false,no_clear_index=false;};
 struct Block{uint32_t outputRate,inputRate;std::vector<int16_t>samples;};
 static constexpr size_t sound_channel_max=8;
 std::atomic<bool>_begun{false},_req_lock{false};std::atomic<void*>_task_handle{nullptr};std::atomic<unsigned>_play_channel_bits{0};
 unsigned begins=0,ends=0;std::vector<Block>blocks;
 Config config()const{return _cfg;}void config(Config c){_cfg=c;}bool isRunning()const{return _begun;}
 bool _begin_raw(){if(!_begun){_begun=true;_task_handle=(void*)1;++begins;}return true;}
 bool begin(){return _begin_raw();}void end(){if(_begun){++ends;_begun=false;_task_handle=nullptr;}}
 void setVolume(uint8_t){}void stop(uint8_t){}bool _in_task(){return false;}void _lock_req(){assert(!_req_lock.exchange(true));}
 int _set_next_wav_locked(size_t,const wav_info_t&i){assert(i.is_stereo&&i.is_16bit&&i.is_signed);auto p=static_cast<const int16_t*>(i.data);blocks.push_back({_cfg.sample_rate,uint32_t(i.sample_rate_x256/256),{p,p+i.length}});return 1;}
 bool _play_raw(const void*,size_t,bool,bool,float,bool,uint32_t,int,bool,bool);
 bool playRaw(const int16_t*p,size_t n,float hz,bool stereo,uint32_t count,uint8_t channel){return _play_raw(p,n,true,true,hz,stereo,count,channel,false,false);}
};
// SDK_PLAY_RAW
struct Expander{bool readRegister(uint8_t,uint8_t*p,size_t){*p=0;return true;}bool digitalWrite(uint8_t,bool){return true;}};
struct Unified{FakeSpeaker Speaker;Expander expander;Expander&getIOExpander(unsigned){return expander;}}M5;
// ACTUAL_HEADER
// ACTUAL_DRIVER
int main(){
 Tab5AudioDriver driver;assert(driver.begin(150,0,6));assert(allocated==0);AudioOutputTab5Speaker output(&driver);
 assert(output.begin());assert(M5.Speaker.begins==1);int16_t zero[2]{};assert(output.ConsumeSample(zero));
 assert(output.SetRate(44100));assert(driver.sourceRate()==44100);assert(M5.Speaker.blocks.size()==1&&M5.Speaker.blocks[0].inputRate==48000&&M5.Speaker.blocks[0].samples.size()==2);assert(M5.Speaker.config().sample_rate==EXPECTED_FIRST_RATE);
 unsigned startupRestarts=M5.Speaker.ends;assert(startupRestarts==EXPECTED_RESTARTS);
 // Rate metadata from the exact SDK _play_raw reaches its request intact.
 for(unsigned rate:{8000U,16000U,22050U,24000U,32000U,44100U,48000U,96000U}){
  assert(output.SetRate(rate));for(unsigned i=0;i<2048;++i){int16_t s[2]={int16_t(i),int16_t(-int(i))};while(!output.ConsumeSample(s)){} }output.flush();
  assert(M5.Speaker.blocks.back().inputRate==rate);assert(M5.Speaker.blocks.back().outputRate==EXPECTED_OUTPUT_RATE);
 }
 const unsigned rateRestarts=M5.Speaker.ends;
 // Format conversion and source-rate boundaries exercised by other PCM clients.
 output.stop();driver.release(Tab5AudioClient::Audio);assert(allocated==0);assert(driver.claim(Tab5AudioClient::Video));
 const uint8_t unsigned8[]={0,128,255};assert(driver.writePcm(Tab5AudioClient::Video,unsigned8,3,{16000,1,Tab5PcmFormat::U8}));assert(driver.flush(Tab5AudioClient::Video));
 auto block=M5.Speaker.blocks.back();assert(block.inputRate==16000&&block.samples.size()==6);assert(block.samples[0]==-32768&&block.samples[1]==-32768&&block.samples[2]==0&&block.samples[4]==32512);
 const uint8_t signed24[]={0,0,0x80,0xff,0xff,0x7f};assert(driver.writePcm(Tab5AudioClient::Video,signed24,1,{44100,2,Tab5PcmFormat::S24}));assert(driver.flush(Tab5AudioClient::Video));block=M5.Speaker.blocks.back();assert(block.samples[0]==-32768&&block.samples[1]==32767);
 const uint8_t signed32[]={0,0,0,0x80,0xff,0xff,0xff,0x7f};assert(driver.writePcm(Tab5AudioClient::Video,signed32,1,{48000,2,Tab5PcmFormat::S32}));assert(driver.flush(Tab5AudioClient::Video));block=M5.Speaker.blocks.back();assert(block.samples[0]==-32768&&block.samples[1]==32767);
 assert(!driver.configure(Tab5AudioClient::Video,{0,2,Tab5PcmFormat::S16}));assert(!driver.configure(Tab5AudioClient::Video,{48000,3,Tab5PcmFormat::S16}));
 driver.release(Tab5AudioClient::Video);assert(allocated==0);assert(driver.owner()==Tab5AudioClient::None);
 for(unsigned i=0;i<100;++i){assert(driver.claim(Tab5AudioClient::Audio));assert(output.SetRate(i%2?44100:48000));assert(output.ConsumeSample(zero));output.flush();output.stop();driver.release(Tab5AudioClient::Audio);assert(allocated==0);}
 std::cout<<"{\"passed\":true,\"sourceRateChanges\":8,\"rateChangeRestarts\":"<<rateRestarts<<",\"startupRestarts\":"<<startupRestarts<<",\"crossClientCycles\":100,\"remainingTrackedAllocations\":"<<allocated<<"}\n";
}
