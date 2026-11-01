#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <alloca.h>
#define CONFIG_IDF_TARGET_ESP32P4 1
#define M5UNIFIED_I2S_HW_V2 1
#define SAMPLERATE_MUL 256
using i2s_port_t=int;using TaskHandle_t=void*;using SemaphoreHandle_t=void*;
constexpr int I2S_NUM_0=0,I2S_PIN_NO_CHANGE=-1;
constexpr uint32_t portMAX_DELAY=0xffffffff;constexpr int pdFALSE=0,pdTRUE=1;
unsigned checks=0; bool joined=true;
#define CHECK(x) do {++checks; if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";std::abort();}}while(0)
// CONFIG
class Speaker_Class {
public:
static constexpr size_t sound_channel_max=8;
// CHANNEL_TYPES
channel_info_t _ch_info[sound_channel_max];
speaker_config_t _cfg;
volatile uint8_t _master_volume=64;
std::atomic<bool> _begun{false},_task_running{false};
std::atomic<uint16_t> _play_channel_bits{0};
std::atomic<TaskHandle_t> _task_handle{nullptr};
SemaphoreHandle_t _task_semaphore=nullptr;
bool (*_cb_set_enabled)(void*,bool)=nullptr;void* _cb_set_enabled_args=nullptr;
void (*_cb_buffer_release)(void*,const void*,uint8_t)=nullptr;void* _cb_buffer_release_args=nullptr;
static void spk_task(void*);void _end_locked();
};
Speaker_Class* active=nullptr; std::vector<int16_t> output;unsigned writes=0,stopWrites=1;
void xSemaphoreGive(SemaphoreHandle_t){} void xTaskNotifyGive(TaskHandle_t){}
uint32_t ulTaskNotifyTake(int,uint32_t){return 1;}
void vTaskDelay(unsigned){CHECK(active&&!active->_task_running);active->_task_handle=nullptr;joined=true;}
void vTaskDelete(void*){joined=true;}
int _spk_i2s_start(i2s_port_t){return 0;}int _spk_i2s_stop(i2s_port_t){return 0;}
int _spk_i2s_driver_uninstall(i2s_port_t){CHECK(joined);return 0;}
int _spk_i2s_write(i2s_port_t,void*data,size_t bytes,size_t*done,uint32_t){
 auto p=static_cast<int16_t*>(data);output.insert(output.end(),p,p+bytes/2);*done=bytes;
 if(++writes>=stopWrites)active->_task_running=false;return 0;
}
// FUNCTIONS
void publish(Speaker_Class&s,const std::vector<int16_t>&data,unsigned rate,bool stereo){
 auto& ch=s._ch_info[0];auto& slot=ch.wavinfo[ch.flip.load()];
 CHECK(slot.state==Speaker_Class::wav_phase_empty);
 slot.info.data=data.data();slot.info.length=data.size();slot.info.repeat=~0u;
 slot.info.sample_rate_x256=rate*256;slot.info.is_16bit=true;slot.info.is_signed=true;slot.info.is_stereo=stereo;
 slot.state=Speaker_Class::wav_phase_published;s._play_channel_bits=1;
}
std::vector<int16_t> render(Speaker_Class&s,unsigned blocks){
 active=&s;writes=0;stopWrites=blocks;output.clear();joined=false;s._task_running=true;s._task_handle=(void*)1;
 Speaker_Class::spk_task(&s);CHECK(joined&&!s._task_running&&s._task_handle==nullptr);return output;
}
void teardown(Speaker_Class&s){
 uint8_t volumes[8];for(unsigned ch=0;ch<8;++ch)volumes[ch]=s._ch_info[ch].volume;
 s._end_locked();CHECK(joined&&!s._play_channel_bits);
 for(unsigned ch=0;ch<8;++ch){CHECK(s._ch_info[ch].volume==volumes[ch]);CHECK(!s._ch_info[ch].current.data);CHECK(!s._ch_info[ch].wavinfo[0].state&&!s._ch_info[ch].wavinfo[1].state);
#if PATCHED
 CHECK(s._ch_info[ch].diff==0&&s._ch_info[ch].index==0);
 for(auto&r:s._ch_info[ch].liner_buf)for(auto v:r)CHECK(v==0);
#endif
 }
}
int main(int argc,char**argv){
 CHECK(argc==2);unsigned cycles=0,contaminated=0;int maxContamination=0;
 std::vector<int16_t> tone(32768,12000),zero(32768,0);
 for(unsigned rate:{8000u,11025u,16000u,22050u,24000u,32000u,44100u,48000u})
 for(unsigned dma:{128u,256u,512u})for(bool stereo:{false,true})for(unsigned cut:{1u,2u,3u,7u}){
  Speaker_Class s;s._cfg.sample_rate=48000;s._cfg.dma_buf_len=dma;s._cfg.stereo=stereo;s._cfg.enable_after_i2s=true;s._master_volume=180;
  for(unsigned ch=0;ch<8;++ch)s._ch_info[ch].volume=uint8_t(137+ch);
  publish(s,tone,rate,stereo);auto initial=render(s,cut);CHECK(std::any_of(initial.begin(),initial.end(),[](int16_t v){return v!=0;}));
  teardown(s);publish(s,zero,rate,stereo);auto silence=render(s,2);
  int maximum=0;for(int16_t v:silence)maximum=std::max(maximum,std::abs(int(v)));
  if(maximum)++contaminated;maxContamination=std::max(maxContamination,maximum);
#if PATCHED
  CHECK(maximum==0);
#endif
  teardown(s);++cycles;
 }
 Speaker_Class cold;cold._cfg.enable_after_i2s=true;publish(cold,zero,44100,false);auto z=render(cold,2);CHECK(std::all_of(z.begin(),z.end(),[](int16_t v){return v==0;}));teardown(cold);
 Speaker_Class continuous;continuous._cfg.enable_after_i2s=true;continuous._cfg.stereo=true;continuous._master_volume=180;
 for(size_t i=0;i<tone.size();++i)tone[i]=int16_t(10000*std::sin(double(i)*0.05));
 publish(continuous,tone,44100,true);auto sequence=render(continuous,64);CHECK(sequence.size()==16384);
 std::ofstream file(argv[1],std::ios::binary);file.write(reinterpret_cast<const char*>(sequence.data()),sequence.size()*2);file.close();teardown(continuous);
#if !PATCHED
 CHECK(contaminated>0);
#endif
 std::cout<<"{\"checks\":"<<checks<<",\"restartCycles\":"<<cycles<<",\"contaminatedSilentRestarts\":"<<contaminated<<",\"maximumStaleSample\":"<<maxContamination<<",\"continuousSamples\":"<<sequence.size()<<",\"physicalAudioTested\":false}\n";
}
