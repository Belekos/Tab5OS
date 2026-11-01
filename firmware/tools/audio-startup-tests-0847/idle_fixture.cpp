#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <climits>
#define M5UNIFIED_I2S_ADC_DAC 1
constexpr unsigned pdFALSE=0, pdTRUE=1, portMAX_DELAY=~0u;
constexpr int GPIO_NUM_26=26,GPIO_NUM_25=25;
struct Config {bool use_dac=false,stereo=false;unsigned dac_zero_level=0,dma_buf_len=256,dma_buf_count=8;int pin_data_out=26;};
struct Self {Config _cfg;} engine;
bool clocks=true;
unsigned starts=0,stops=0,waits=0,writes=0,checks=0;
#define CHECK(x) do{++checks;assert(x);}while(0)
unsigned ulTaskNotifyTake(unsigned,unsigned timeout){if(timeout==portMAX_DELAY){++waits;return 1;}return 0;}
void _spk_i2s_stop(int){CHECK(clocks);clocks=false;++stops;}
void _spk_i2s_start(int){CHECK(!clocks);clocks=true;++starts;}
void _spk_i2s_set_dac(int,bool,bool){}
void _spk_i2s_write(int,void*data,size_t size,size_t*done,unsigned){
 CHECK(clocks);const auto p=static_cast<uint8_t*>(data);
 CHECK(std::all_of(p,p+size,[](uint8_t b){return b==0;}));*done=size;++writes;
}
void run(bool internalDac){
 auto*self=&engine;self->_cfg.use_dac=internalDac;
 const size_t dma_buf_len=self->_cfg.dma_buf_len;
 const int32_t spk_sample_rate_x256=48000*256;
 const bool out_stereo=self->_cfg.stereo;
 const int i2s_port=0;
 int32_t dac_offset=0;
 enum spk_i2s_state {spk_i2s_stop,spk_i2s_mute,spk_i2s_run};
 spk_i2s_state flg_i2s_started=spk_i2s_run;
 uint8_t buf_cnt=1;bool flg_nodata=true;
 int32_t sound_buf32[256]{};
 // ACTUAL_IDLE
 CHECK(waits>0);
 CHECK(clocks==!internalDac);
 // First work after the native notification wait. No external-codec clock
 // restart may occur while that codec remains enabled after the prime.
 // ACTUAL_FIRST_WORK
 CHECK(clocks);CHECK(flg_i2s_started==spk_i2s_run);
}
int main(){
 for(unsigned i=0;i<1000;++i)run(false);
 CHECK(starts==0&&stops==0&&waits==1000);
 unsigned externalWrites=writes;
 run(true);CHECK(starts==1&&stops==1);
 std::cout<<"{\"checks\":"<<checks<<",\"externalIdleResumeCycles\":1000,\"externalClockRestarts\":0,\"nativeZeroWrites\":"<<externalWrites<<",\"internalDacControlRestarts\":1}\n";
}
