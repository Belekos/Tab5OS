#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define CONFIG_IDF_TARGET_ESP32P4 1
#define portNUM_PROCESSORS 2
using i2s_port_t = int;
using TaskHandle_t = void*;
using SemaphoreHandle_t = void*;
using esp_err_t = int;
constexpr int I2S_PIN_NO_CHANGE=-1, I2S_NUM_0=0, GPIO_NUM_26=26;
constexpr int ESP_OK=0, ESP_FAIL=-1, pdPASS=1, wav_phase_empty=0;
constexpr uint8_t es8388_i2c_addr=0x10, pi4io1_i2c_addr=0x43;
unsigned checks=0;
#define CHECK(v) do { ++checks; if (!(v)) { std::cerr << "check failed line " << __LINE__ << ": " << #v << "\n"; std::abort(); } } while(0)

struct Hardware {
  bool allocated=false, clocks=false, amp=false, muted=false, badEnable=false;
  bool failSetup=false, failStart=false, failInfo=false, failTask=false, failSemaphore=false, shortWrites=false;
  int failWrite=-1, failI2c=-1;
  unsigned writes=0, i2c=0, taskCreates=0, hardwareStarts=0;
  size_t zeroBytes=0, dmaBytes=0;
  std::vector<std::string> events;
  bool io() { return int(++i2c)!=failI2c; }
} hw;
// CONFIGURATION
class Speaker_Class {
 public:
  speaker_config_t _cfg;
  struct Empty { void clear() {} };
  struct Slot { Empty info; std::atomic<int> state{0}; };
  struct Channel { Slot wavinfo[2]; Empty current; } _ch_info[8];
  static constexpr size_t sound_channel_max=8;
  std::atomic<bool> _begun{false}, _begin_lock{false}, _req_lock{false}, _task_running{false};
  std::atomic<unsigned> _play_channel_bits{0};
  std::atomic<TaskHandle_t> _task_handle{nullptr};
  SemaphoreHandle_t _task_semaphore=nullptr;
  bool (*_cb_set_enabled)(void*,bool)=nullptr;
  void* _cb_set_enabled_args=nullptr;
  speaker_config_t config() const { return _cfg; }
  void _lock_req() { CHECK(!_req_lock.exchange(true)); }
  bool begin(); bool _begin_raw(); void end(); void _end_locked();
  esp_err_t _setup_i2s() {
    hw.events.push_back("configure");
    if(hw.failSetup)return ESP_FAIL;
    hw.allocated=true;
    hw.dmaBytes=((_cfg.dma_buf_len*(_cfg.stereo?4u:2u)+31u)/32u)*32u*_cfg.dma_buf_count;
    return ESP_OK;
  }
  static void spk_task(void*) {}
};
Speaker_Class* active=nullptr;
struct i2s_chan_info_t { size_t total_dma_buf_size=0; };
void* _spk_i2s_handle[1]={(void*)1};
int i2s_channel_get_info(void*,i2s_chan_info_t*info){info->total_dma_buf_size=hw.dmaBytes;return hw.failInfo?ESP_FAIL:ESP_OK;}

SemaphoreHandle_t xSemaphoreCreateBinary(){return hw.failSemaphore?nullptr:(void*)1;}
void vTaskDelay(unsigned) {
  if(active && !active->_task_running.load()) {
    hw.clocks=false;
    active->_task_handle=nullptr;
  }
}
void xTaskNotifyGive(TaskHandle_t) {}
int _spk_i2s_start(i2s_port_t){
  hw.events.push_back("clock-start");++hw.hardwareStarts;
  if(hw.failStart||!hw.allocated)return ESP_FAIL;
  hw.clocks=true;return ESP_OK;
}
int _spk_i2s_stop(i2s_port_t){hw.events.push_back("clock-stop");hw.clocks=false;return ESP_OK;}
int _spk_i2s_driver_uninstall(i2s_port_t){
  CHECK(!hw.clocks);hw.events.push_back("uninstall");hw.allocated=false;return ESP_OK;
}
int _spk_i2s_write(i2s_port_t,void*data,size_t bytes,size_t*done,unsigned timeout){
  CHECK(hw.clocks); CHECK(timeout==1000);
  const auto p=static_cast<const uint8_t*>(data);
  CHECK(std::all_of(p,p+bytes,[](uint8_t b){return b==0;}));
  ++hw.writes;*done=0;
  if(int(hw.writes)==hw.failWrite)return ESP_FAIL;
  *done=hw.shortWrites?std::max<size_t>(1,bytes/2):bytes;
  hw.zeroBytes+=*done;hw.events.push_back("zero-dma");return ESP_OK;
}
int xTaskCreatePinnedToCore(void(*)(void*),const char*,size_t,void*arg,unsigned,TaskHandle_t*out,unsigned){
  ++hw.taskCreates;hw.events.push_back("task-create");
  if(hw.failTask){*out=nullptr;return 0;}
  active=static_cast<Speaker_Class*>(arg);*out=(void*)1;
  // Boundary: real spk_task starts I2S on its first loop unless begin primed it.
  if(!active->_cfg.enable_after_i2s) CHECK(_spk_i2s_start(0)==ESP_OK);
  return pdPASS;
}
int xTaskCreate(void(*fn)(void*),const char*n,size_t s,void*a,unsigned p,TaskHandle_t*h){return xTaskCreatePinnedToCore(fn,n,s,a,p,h,0);}
namespace m5gfx {void delay(unsigned) {}}
struct Bus {
  bool bitOff(uint8_t,uint8_t,uint8_t,unsigned){
    bool ok=hw.io();hw.events.push_back("amp-off");if(ok)hw.amp=false;return ok;
  }
  bool bitOn(uint8_t,uint8_t,uint8_t,unsigned){
    bool ok=hw.io();hw.events.push_back("amp-on");if(ok){hw.amp=true;if(!hw.clocks||hw.muted)hw.badEnable=true;}return ok;
  }
  bool writeRegister8(uint8_t,uint8_t reg,uint8_t value,unsigned){
    bool ok=hw.io();if(!ok)return false;
    if(reg==25){hw.muted=(value&4)!=0;hw.events.push_back(hw.muted?"mute":"unmute");if(!hw.muted&&!hw.clocks)hw.badEnable=true;}
    if(reg==4 && value==0x3c)hw.events.push_back("dac-power");
    if(reg==0 && value==0x80)hw.events.push_back("codec-reset");
    return true;
  }
  bool writeRegister(uint8_t addr,uint8_t reg,const uint8_t*data,unsigned n,unsigned freq){
    bool ok=true;for(unsigned i=0;i<n;++i)ok=writeRegister8(addr,reg+i,data[i],freq)&&ok;return ok;
  }
};
class M5Unified {
 public:
  Speaker_Class Speaker;
  Bus In_I2C;
  static bool _speaker_enabled_cb_tab5(void*,bool);
} M5;
// BULK_WRITE
// CODEC_CALLBACK
// SPEAKER_METHODS

void reset(){
  CHECK(!M5.Speaker._task_running);CHECK(!M5.Speaker._begun);
  hw=Hardware{};active=&M5.Speaker;
  M5.Speaker._cfg=speaker_config_t{};
  M5.Speaker._cfg.pin_data_out=26;
  M5.Speaker._cfg.enable_after_i2s=PATCHED;
  M5.Speaker._cb_set_enabled=M5Unified::_speaker_enabled_cb_tab5;
  M5.Speaker._cb_set_enabled_args=&M5;
}
size_t event(const char*s){auto p=std::find(hw.events.begin(),hw.events.end(),s);return size_t(p-hw.events.begin());}
int main(){
  unsigned cycles=0,failedCases=0;
  for(bool stereo:{false,true})for(unsigned count:{2u,8u,12u})for(unsigned len:{113u,128u,256u,512u}){
    reset();M5.Speaker._cfg.stereo=stereo;M5.Speaker._cfg.dma_buf_count=count;M5.Speaker._cfg.dma_buf_len=len;
    CHECK(M5.Speaker.begin());CHECK(M5.Speaker._begun);CHECK(hw.amp&&!hw.muted);
    CHECK(hw.badEnable==!PATCHED);
    if(PATCHED){
      CHECK(hw.zeroBytes==hw.dmaBytes+hw.dmaBytes/count);
      CHECK(event("clock-start")<event("codec-reset"));
      CHECK(event("zero-dma")<event("codec-reset"));
      CHECK(event("unmute")<event("amp-on"));
      CHECK(event("amp-on")<event("task-create"));
      CHECK(event("mute")<event("dac-power"));
    }
    const unsigned starts=hw.hardwareStarts;CHECK(M5.Speaker.begin());CHECK(hw.hardwareStarts==starts);
    M5.Speaker.end();CHECK(!hw.allocated&&!hw.clocks&&!hw.amp);++cycles;
  }
  unsigned failurePoints=0;
  reset();CHECK(M5.Speaker.begin());failurePoints=hw.i2c;M5.Speaker.end();
  if(PATCHED){
    for(unsigned i=1;i<=failurePoints;++i){
      reset();hw.failI2c=i;CHECK(!M5.Speaker.begin());
      CHECK(!M5.Speaker._begun&&!M5.Speaker._task_running);
      CHECK(!hw.allocated&&!hw.clocks&&!hw.amp);++failedCases;
    }
    for(unsigned failure=0;failure<5;++failure){
      reset();hw.failSetup=failure==0;hw.failStart=failure==1;hw.failWrite=failure==2?2:-1;hw.failTask=failure==3;hw.failInfo=failure==4;
      CHECK(!M5.Speaker.begin());CHECK(!hw.allocated&&!hw.clocks&&!hw.amp);++failedCases;
    }
    reset();hw.shortWrites=true;CHECK(M5.Speaker.begin());CHECK(hw.zeroBytes==9*256*2);M5.Speaker.end();
    reset();M5.Speaker._task_semaphore=nullptr;hw.failSemaphore=true;
    CHECK(!M5.Speaker.begin());CHECK(hw.i2c==0&&!hw.allocated&&!hw.clocks);
    CHECK(!M5.Speaker._begin_lock&&!M5.Speaker._req_lock);++failedCases;
    hw.failSemaphore=false;CHECK(M5.Speaker.begin());M5.Speaker.end();
    // The feature is opt-in; another board's normal path keeps its ordering.
    reset();M5.Speaker._cfg.enable_after_i2s=false;CHECK(M5.Speaker.begin());CHECK(hw.zeroBytes==0);CHECK(event("codec-reset")<event("configure"));M5.Speaker.end();
  }
  std::cout<<"{\"checks\":"<<checks<<",\"startupCycles\":"<<cycles<<",\"failureCases\":"<<failedCases<<",\"enableBeforeClock\":"<<(!PATCHED?cycles:0)<<",\"physicalAudioTested\":false}\n";
}
