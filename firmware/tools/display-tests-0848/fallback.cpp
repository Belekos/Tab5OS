#include "platform.h"
#include "Tab5DisplayPresent.h"
#include "Tab5DisplayCache.h"
#include <sys/mman.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
M5Mock M5;
void tab5DisplayBandwidthInit() {}
int Cache_WriteBack_Addr(uint32_t,uint32_t);
constexpr size_t Bytes=720*1280*2;
static uint8_t* fb[2];static uint8_t* rows[1280];static unsigned checks,syncs,publishes;static bool failAllocation=false,noSecond=false;
static esp_lcd_dpi_panel_event_callbacks_t callbacks{};
static std::vector<uint8_t> memory[2];
static esp_lcd_panel_t handle{};
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": "<<#x<<"\n";abort();}}while(0)
esp_lcd_panel_handle_t testDriverHandle(){return &handle;}
TaskHandle_t xTaskGetCurrentTaskHandle(){return reinterpret_cast<void*>(1);}
void vTaskDelay(unsigned){abort();}int64_t esp_timer_get_time(){return 0;}
bool esp_ptr_external_ram(const void*){return true;}
void* heap_caps_malloc(size_t n,int){return failAllocation?nullptr:malloc(n);}void heap_caps_free(void*p){free(p);}
int esp_cache_msync(void*p,size_t n,int flags){CHECK(flags==(ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_TYPE_DATA));CHECK(n<=16384&&n%128==0);++syncs;for(unsigned b=0;b<2;++b)if(uintptr_t(p)>=uintptr_t(fb[b])&&uintptr_t(p)+n<=uintptr_t(fb[b])+Bytes){size_t off=uintptr_t(p)-uintptr_t(fb[b]);memcpy(memory[b].data()+off,p,n);return ESP_OK;}CHECK(false);return ESP_ERR_INVALID_ARG;}
int esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t,unsigned n,void**a,void**b){CHECK(n==2);*a=fb[0];*b=noSecond?nullptr:fb[1];return ESP_OK;}
int esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t,const esp_lcd_dpi_panel_event_callbacks_t*c,void*){callbacks=*c;return ESP_OK;}
int esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t,int,int,int,int,const void*){++publishes;return ESP_OK;}
int main(int argc,char**argv){CHECK(argc==2);bool cacheFailure=strcmp(argv[1],"cache-oom")==0;CHECK(cacheFailure||strcmp(argv[1],"presenter-unavailable")==0);
 for(unsigned i=0;i<2;++i){fb[i]=static_cast<uint8_t*>(mmap(nullptr,Bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0));CHECK(fb[i]!=MAP_FAILED);memset(fb[i],i?0xcc:0x11,Bytes);memory[i].assign(fb[i],fb[i]+Bytes);}
 for(unsigned y=0;y<1280;++y)rows[y]=fb[0]+y*1440;M5.Display.panel.init(fb[0],rows);
 // Match OS order: cache setup precedes presenter setup. Failed metadata must
 // not turn a formerly operational single-buffer path into an unusable UI.
 failAllocation=cacheFailure;CHECK(tab5DisplayCacheInit()==!cacheFailure);failAllocation=false;
 noSecond=!cacheFailure;CHECK(tab5DisplayPresentInit()==cacheFailure);
 CHECK(tab5DisplayCacheBeginFrame());if(cacheFailure)CHECK(syncs>0);else CHECK(syncs==0);
 tab5DisplayCacheDirtyRect(10,30,20,15);auto*b=static_cast<uint8_t*>(M5.Display.panel.config_detail().buffer);
 std::vector<uint8_t> input(20*15*2,0x85);CHECK(!tab5DisplayPresentWritePixels(10,30,20,15,input.data(),input.size()));for(unsigned y=30;y<45;++y)memset(b+y*1440+20,0x85,40);
 auto before=syncs;CHECK(Cache_WriteBack_Addr(uint32_t(uintptr_t(b+30*1440+20)),14*1440+40)==ESP_OK);CHECK(syncs>before);
 tab5DisplayCacheEndFrame();CHECK(publishes==unsigned(cacheFailure));unsigned selected=cacheFailure?1:0;
 CHECK(memory[selected][30*1440+20]==0x85);CHECK(memory[selected][29*1440+20]==0x11);
 std::cout<<"{\"checks\":"<<checks<<",\"mode\":\""<<argv[1]<<"\",\"synchronousFallback\":true,\"publications\":"<<publishes<<"}"<<std::endl;
 for(auto*p:fb)munmap(p,Bytes);
}
