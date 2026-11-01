#include "platform.h"
#include "Tab5DisplayPresent.h"
#include "Tab5DisplayCache.h"
#include "Tab5DisplayCacheRanges.h"
#include <sys/mman.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstddef>
using esp_lcd_dsi_bus_handle_t=void*;using lcd_color_format_t=int;using dw_gdma_channel_handle_t=void*;using intr_handle_t=void*;using dw_gdma_link_list_handle_t=void*;using esp_async_fbcpy_handle_t=void*;using SemaphoreHandle_t=void*;using esp_pm_lock_handle_t=void*;
using esp_lcd_dpi_panel_color_trans_done_cb_t=bool(*)(esp_lcd_panel_handle_t,esp_lcd_dpi_panel_event_data_t*,void*);using esp_lcd_dpi_panel_refresh_done_cb_t=esp_lcd_dpi_panel_color_trans_done_cb_t;
constexpr unsigned DPI_PANEL_MAX_FB_NUM=3;
// SDK_STRUCT
static esp_lcd_dpi_panel_t driver{};
M5Mock M5;
esp_lcd_panel_handle_t testDriverHandle(){return &driver.base;}
void tab5DisplayBandwidthInit(){}
constexpr size_t Bytes=720*1280*2;
static uint8_t* fb[2]{};static uint8_t* rows[1280];static uint8_t* scanning=nullptr;
static std::vector<uint8_t> memory[2];static unsigned checks=0,cacheCalls=0,dmaRestarts=0,publishes=0;static uint64_t cacheBytes=0;static bool failCache=false,failPublish=false;static uint32_t nowMs=0;static TaskHandle_t currentTask=reinterpret_cast<void*>(1);static bool dmaBeforeReturn=false,dmaDuringDriverCache=false;
void dma();
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"check "<<__LINE__<<": "<<#x<<"\n";abort();}}while(0)
TaskHandle_t xTaskGetCurrentTaskHandle(){return currentTask;}void vTaskDelay(unsigned){abort();}int64_t esp_timer_get_time(){return uint64_t(nowMs)*1000;}
bool esp_ptr_external_ram(const void*){return true;}void*heap_caps_malloc(size_t n,int){return malloc(n);}void heap_caps_free(void*p){free(p);}
int esp_cache_msync(void* p,size_t n,int flags){
 CHECK(flags==(ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_TYPE_DATA)||flags==(ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_UNALIGNED));
 int k=-1;for(int i=0;i<2;++i)if(uintptr_t(p)>=uintptr_t(fb[i])&&uintptr_t(p)+n<=uintptr_t(fb[i])+Bytes)k=i;
 CHECK(k>=0);CHECK(scanning!=fb[k]);
 if(flags&ESP_CACHE_MSYNC_FLAG_UNALIGNED){if(dmaDuringDriverCache){dmaDuringDriverCache=false;dma();}}
 ++cacheCalls;cacheBytes+=n;if(failCache){failCache=false;return ESP_ERR_INVALID_STATE;}auto off=uintptr_t(p)-uintptr_t(fb[k]);memcpy(memory[k].data()+off,p,n);return ESP_OK;
}
int esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t h,unsigned n,void**a,void**b){CHECK(h==&driver.base&&n==2);*a=fb[0];*b=fb[1];return ESP_OK;}
int esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t h,const esp_lcd_dpi_panel_event_callbacks_t*c,void*){CHECK(h==&driver.base);driver.on_color_trans_done=c->on_color_trans_done;driver.on_refresh_done=c->on_refresh_done;return ESP_OK;}
#define __containerof(ptr,type,member) ((type*)((char*)(ptr)-offsetof(type,member)))
#define MAX(a,b) std::max(a,b)
#define MIN(a,b) std::min(a,b)
#define ESP_LOGV(...) ((void)0)
#define ESP_RETURN_ON_FALSE(x,err,...) do{if(!(x))return err;}while(0)
#define ESP_RETURN_ON_ERROR(x,...) do{int e=(x);if(e)return e;}while(0)
constexpr int pdTRUE=1;
int xSemaphoreTake(void*,int){abort();}
struct esp_async_fbcpy_event_data_t{};
struct esp_async_fbcpy_trans_desc_t{const void*src_buffer;void*dst_buffer;int src_buffer_size_x,src_buffer_size_y;uint32_t dst_buffer_size_x,dst_buffer_size_y;int src_offset_x,src_offset_y,dst_offset_x,dst_offset_y,copy_size_x,copy_size_y;struct{int color_type_id;}pixel_format_unique_id;};
bool async_fbcpy_done_cb(esp_async_fbcpy_handle_t,esp_async_fbcpy_event_data_t*,void*){abort();}
int esp_async_fbcpy(esp_async_fbcpy_handle_t,const esp_async_fbcpy_trans_desc_t*,bool(*)(esp_async_fbcpy_handle_t,esp_async_fbcpy_event_data_t*,void*),void*){abort();}
// SDK_DRAW
struct dw_gdma_trans_done_event_data_t{};struct dw_gdma_block_markers_t{bool is_valid,is_last;};
void* dw_gdma_link_list_get_item(void*list,int i){CHECK(i==0);return list;}
void dw_gdma_lli_set_block_markers(void*p,dw_gdma_block_markers_t m){CHECK((p==fb[0]||p==fb[1])&&m.is_valid&&m.is_last);}
void dw_gdma_channel_use_link_list(void*,void*link){scanning=static_cast<uint8_t*>(link);++dmaRestarts;}
void dw_gdma_channel_enable_ctrl(void*,bool b){CHECK(b);}
#define MIPI_DSI_BRG_LL_EVENT_VSYNC 0
// SDK_DMA
int esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t h,int x,int y,int xe,int ye,const void*p){CHECK(h==&driver.base&&x==0&&y==0&&xe==720&&ye==1);if(failPublish){failPublish=false;return ESP_ERR_INVALID_STATE;}++publishes;auto result=dpi_panel_draw_bitmap(h,x,y,xe,ye,p);if(dmaBeforeReturn){dmaBeforeReturn=false;dma();}return result;}
int Cache_WriteBack_Addr(uint32_t,uint32_t);
void dma(){CHECK(driver.on_refresh_done);mipi_dsi_dma_trans_done_cb(nullptr,nullptr,&driver);++nowMs;}
static void flushRect(unsigned x,unsigned y,unsigned w,unsigned h,uint8_t value){tab5DisplayCacheDirtyRect(x,y,w,h);for(unsigned row=y;row<y+h;++row)memset(M5.Display.panel.row(row)+x*2,value,w*2);auto*draw=static_cast<uint8_t*>(M5.Display.panel.config_detail().buffer);CHECK(Cache_WriteBack_Addr(uint32_t(uintptr_t(draw+y*1440+x*2)),(h-1)*1440+w*2)==ESP_OK);}
namespace lgfx {
struct pixelcopy_t {uint32_t src_x32_add=65536,src_y32_add=0,src_x32=0,src_y32=0;};
struct Panel_FrameBufferBase {
 struct Config{uint16_t panel_width=720,panel_height=1280;uint8_t offset_rotation=0;} _cfg;
 uint8_t _rotation=0,_internal_rotation=0;uint_fast16_t _width=720,_height=1280,_xe=719,_ye=1279,_xs=0,_ys=0;
 void setRotation(uint_fast8_t);void _rotate_pixelcopy(uint_fast16_t&,uint_fast16_t&,uint_fast16_t&,uint_fast16_t&,pixelcopy_t*,uint32_t&,uint32_t&);
};
// M5_ROTATION
}
static void rotations(){
 lgfx::Panel_FrameBufferBase panel;Tab5DisplayCacheRanges::Geometry g{};Tab5DisplayCacheRanges::Row r[1280];
 for(unsigned rotation=0;rotation<8;++rotation)for(unsigned offset=0;offset<8;++offset){
  panel._cfg.offset_rotation=offset;panel.setRotation(rotation);
  CHECK(Tab5DisplayCacheRanges::configure(g,0x40000000,720,1280,1440,rotation,offset));
  CHECK(g.logicalWidth==panel._width&&g.logicalHeight==panel._height);
  for(unsigned n=0;n<200;++n){
   unsigned x=(n*157)%g.logicalWidth,y=(n*283)%g.logicalHeight,w=std::min(1+n%50,unsigned(g.logicalWidth-x)),h=std::min(1+n%60,unsigned(g.logicalHeight-y));
   Tab5DisplayCacheRanges::clear(g,r);CHECK(Tab5DisplayCacheRanges::addRect(g,r,x,y,w,h));
   uint_fast16_t px=x,py=y,pw=w,ph=h;lgfx::pixelcopy_t copy;uint32_t nx=0,ny=65536;
   panel._rotate_pixelcopy(px,py,pw,ph,&copy,nx,ny);
   for(unsigned row=0;row<1280;++row){if(row>=py&&row<py+ph){CHECK(r[row].first==px&&r[row].end==px+pw);}else CHECK(r[row].first==UINT16_MAX&&r[row].end==0);}
  }
 }
}
static std::string configuration(){
 std::string out;tab5DisplayPresentDumpConfiguration([](const char*p,size_t n,void*c){static_cast<std::string*>(c)->append(p,n);},&out);return out;
}
int main(){rotations();CHECK(configuration()=="[DISPLAY config] available=0\n");
 for(unsigned i=0;i<2;++i){fb[i]=static_cast<uint8_t*>(mmap(nullptr,Bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0));CHECK(fb[i]!=MAP_FAILED);memset(fb[i],i?0xcc:0x11,Bytes);memory[i].assign(fb[i],fb[i]+Bytes);driver.fbs[i]=fb[i];driver.link_lists[i]=fb[i];}
 driver.cur_fb_index=0;driver.num_fbs=2;driver.h_pixels=720;driver.v_pixels=1280;driver.fb_size=Bytes;driver.bits_per_pixel=16;
 for(unsigned y=0;y<1280;++y)rows[y]=fb[0]+y*1440;
 scanning=fb[0];M5.Display.panel.init(fb[0],rows);
 CHECK(tab5DisplayPresentInit());CHECK(tab5DisplayCacheInit());
 auto baselineConfig=configuration();CHECK(baselineConfig.find("available=1 requestedPixelClockMHz=80 width=720 height=1280")!=std::string::npos);
 CHECK(baselineConfig.find("hBack=140 hPulse=40 hFront=40 vBack=20 vPulse=4 vFront=20")!=std::string::npos);
 CHECK(configuration()==baselineConfig);CHECK(configuration()==baselineConfig);CHECK(cacheCalls==0&&publishes==0);
 currentTask=reinterpret_cast<void*>(2);CHECK(configuration()=="[DISPLAY config] available=0\n");currentTask=reinterpret_cast<void*>(1);
 M5.board=m5::boardOther;CHECK(configuration()=="[DISPLAY config] available=0\n");M5.board=m5::board_M5Tab5;
 M5.Display.panel.bus.type=lgfx::bus_type_t::bus_spi;CHECK(configuration()=="[DISPLAY config] available=0\n");M5.Display.panel.bus.type=lgfx::bus_type_t::bus_dsi;
 tab5DisplayPresentDumpConfiguration(nullptr);CHECK(cacheCalls==0&&publishes==0);

 std::vector<uint8_t> expected(fb[0],fb[0]+Bytes);
 // Actual IDF draw only changes desiredindex; actual DMA callback selects that
 // fullframe descriptor BEFORE its refresh callback. Host uses its exactbody.
 for(unsigned n=0;n<500;++n){
  CHECK(tab5DisplayCacheBeginFrame());auto st=tab5DisplayPresentStats();CHECK(scanning==fb[st.front]);
  auto before=memory[st.front];unsigned x=(n*17)%680,y=(n*31)%1240,w=1+n%30,h=1+n%25;uint8_t v=uint8_t(1+n%240);
  flushRect(x,y,w,h,v);for(unsigned row=y;row<y+h;++row)memset(expected.data()+row*1440+x*2,v,w*2);
  tab5DisplayCacheEndFrame();CHECK(driver.cur_fb_index==st.back);CHECK(scanning==fb[st.front]);CHECK(memory[st.front]==before);
  CHECK(memory[st.back]==expected); // pixels outside firstdriverrow were synced by productioncache, not IDF draw
  dma();CHECK(scanning==fb[st.back]);CHECK(!tab5DisplayPresentReady()); // conservative second event remains required
  dma();CHECK(tab5DisplayPresentReady());
 }
 auto oldCalls=cacheCalls,oldPublishes=publishes;
 for(unsigned n=0;n<1000;++n){dma();CHECK(tab5DisplayPresentReady());CHECK(!tab5DisplayCacheNeedsRedraw());}
 CHECK(cacheCalls==oldCalls&&publishes==oldPublishes);
 // Interrupt interleavings inside the real draw call: beforecurfb assignment,
 // and afterthat assignment but before the application snapshots eventcount.
 for(unsigned which=0;which<2;++which){
  CHECK(tab5DisplayCacheBeginFrame());flushRect(10,300,60,30,uint8_t(which+0x61));
  dmaDuringDriverCache=which==0;dmaBeforeReturn=which==1;
  tab5DisplayCacheEndFrame();CHECK(!tab5DisplayPresentReady());
  dma();CHECK(!tab5DisplayPresentReady());dma();CHECK(tab5DisplayPresentReady());
 }
 // An interrupt immediately after submission's curfb update but before its
 // following taskpoll is harmless: neither renderer nor cache writes the scan.
 CHECK(tab5DisplayCacheBeginFrame());flushRect(0,700,720,120,0x73);tab5DisplayCacheEndFrame();dma();dma();dma();CHECK(tab5DisplayPresentReady());
 // Repeated full-page updates reproduce mirror/draw overlap without timing assumptions.
 auto fullBefore=cacheBytes;auto fullCallsBefore=cacheCalls;
 for(unsigned n=0;n<40;++n){CHECK(tab5DisplayCacheBeginFrame());flushRect(0,0,720,1280,uint8_t(n+1));tab5DisplayCacheEndFrame();dma();dma();CHECK(tab5DisplayPresentReady());}
 auto fullBytes=cacheBytes-fullBefore;auto fullCalls=cacheCalls-fullCallsBefore;
 // A cache failure on drawing is detected before publication in either implementation.
 auto visible=memory[scanning==fb[0]?0:1];auto successfulPublishes=publishes;
 CHECK(tab5DisplayCacheBeginFrame());tab5DisplayCacheDirtyRect(40,350,20,10);
 auto* draw=static_cast<uint8_t*>(M5.Display.panel.config_detail().buffer);
 for(unsigned y=350;y<360;++y)memset(draw+y*1440+80,0xac,40);
 failCache=true;(void)Cache_WriteBack_Addr(uint32_t(uintptr_t(draw+350*1440+80)),9*1440+40);
 tab5DisplayCacheEndFrame();CHECK(!failCache);CHECK(publishes==successfulPublishes);CHECK(memory[scanning==fb[0]?0:1]==visible);
 nowMs+=1000;CHECK(tab5DisplayCacheNeedsRedraw());CHECK(tab5DisplayCacheBeginFrame());flushRect(0,0,720,1280,0x67);tab5DisplayCacheEndFrame();dma();dma();CHECK(tab5DisplayPresentReady());
 CHECK(memory[scanning==fb[0]?0:1]==std::vector<uint8_t>(Bytes,0x67));
 // Publication failure must preserve scan ownership, then restore a complete back image.
 successfulPublishes=publishes;visible=memory[scanning==fb[0]?0:1];
 CHECK(tab5DisplayCacheBeginFrame());flushRect(100,400,10,5,0x98);failPublish=true;tab5DisplayCacheEndFrame();
 CHECK(!failPublish&&publishes==successfulPublishes);CHECK(memory[scanning==fb[0]?0:1]==visible);
 nowMs+=1000;CHECK(tab5DisplayCacheNeedsRedraw());CHECK(tab5DisplayCacheBeginFrame());flushRect(0,0,720,1280,0x79);tab5DisplayCacheEndFrame();dma();dma();CHECK(tab5DisplayPresentReady());
 CHECK(memory[scanning==fb[0]?0:1]==std::vector<uint8_t>(Bytes,0x79));
 for(auto*p:fb)munmap(p,Bytes);
 std::cout<<"{\"checks\":"<<checks<<",\"rotationCombinations\":64,\"rotationRectangles\":12800,\"partialFrames\":500,\"dmaRestarts\":"<<dmaRestarts<<",\"publications\":"<<publishes<<",\"cacheCalls\":"<<cacheCalls<<",\"cacheBytes\":"<<cacheBytes<<",\"repeatedFullFrameCacheBytes\":"<<fullBytes<<",\"repeatedFullFrameCacheCalls\":"<<fullCalls<<",\"idleNewPublications\":0,\"scanningWrites\":0}"<<std::endl;
}
