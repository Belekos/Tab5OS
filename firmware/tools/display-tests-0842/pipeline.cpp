#include <lvgl.h>
#include "platform.h"
#include "Tab5DisplayPresent.h"
#include "Tab5DisplayCache.h"
#include "Tab5DisplayActivity.h"
#include "Tab5DisplayRefresh.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <vector>

// Three distinct views are essential: CPU-cache bytes, external-RAM bytes,
// and the framebuffer consumed by DMA. The old tests collapsed all three.
// This is a deterministic software model, not an emulation of PSRAM bandwidth.
M5Mock M5;
void tab5DisplayBandwidthInit() {}
int Cache_WriteBack_Addr(uint32_t, uint32_t);
static constexpr unsigned W=720,H=1280,Stride=W*2,Bytes=Stride*H;
static uint8_t* cpu[2]{};
static uint8_t* rows[H]{};
static std::vector<uint8_t> ram[2],reference(Bytes),published(Bytes);
static unsigned checks,submissions,syncs,scans,flushes,frames;
static uint32_t clockMs,nextDma=16;
static int chosen=0,scanning=0,cacheError=0,publishError=0;
static bool paused=false,transaction=false,drawable=false,hasPublished=false;
static esp_lcd_dpi_panel_event_callbacks_t callbacks;
static void* owner=reinterpret_cast<void*>(1);
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"pipeline check %u line %d: %s\n",checks,__LINE__,#x);abort();}}while(0)
uint32_t millis(){return clockMs;}
TaskHandle_t xTaskGetCurrentTaskHandle(){return owner;}
int64_t esp_timer_get_time(){return int64_t(clockMs)*1000;}
bool esp_ptr_external_ram(const void*){return true;}
void* heap_caps_malloc(size_t n,int){return malloc(n);}
void heap_caps_free(void* p){free(p);}
static void dmaDone(){
 // Exact IDF5.5.4 ordering on P4 rev<3: read cur_fb_index, select its
 // descriptor, restart DMA, then deliver on_refresh_done.
 scanning=chosen;++scans;
 CHECK(callbacks.on_refresh_done);callbacks.on_refresh_done(nullptr,nullptr,nullptr);
 if(hasPublished)CHECK(ram[scanning]==published);
}
static void advance(unsigned n){
 for(unsigned i=0;i<n;++i){++clockMs;if(clockMs>=nextDma){nextDma=clockMs+16;if(!paused)dmaDone();}}
}
void vTaskDelay(unsigned n){advance(n);}
int esp_cache_msync(void* address,size_t n,int flags){
 CHECK(flags==(ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_TYPE_DATA));
 CHECK((uintptr_t(address)&127)==0&&n&&n<=16384&&n%128==0);++syncs;
 if(cacheError&&transaction)return cacheError;
 for(unsigned b=0;b<2;++b)if(address>=cpu[b]&&static_cast<uint8_t*>(address)<cpu[b]+Bytes){
  const size_t off=static_cast<uint8_t*>(address)-cpu[b];CHECK(off+n<=Bytes);
  // A writeback must never change any externally visible scanning pixels.
  if(int(b)==scanning)CHECK(!memcmp(ram[b].data()+off,cpu[b]+off,n));
  memcpy(ram[b].data()+off,cpu[b]+off,n);return ESP_OK;
 }
 CHECK(false);return ESP_ERR_INVALID_ARG;
}
int esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t,unsigned n,void** a,void** b){CHECK(n==2);*a=cpu[0];*b=cpu[1];return ESP_OK;}
int esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t,const esp_lcd_dpi_panel_event_callbacks_t* c,void*){callbacks=*c;return ESP_OK;}
int esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t,int x,int y,int w,int h,const void* p){
 CHECK(x==0&&y==0&&w==720&&h==1);if(publishError)return publishError;
 int b=p==cpu[0]?0:p==cpu[1]?1:-1;CHECK(b>=0&&b!=scanning);
 // IDF's owned-buffer branch synchronizes the requested row, then changes
 // cur_fb_index. No pixel copy and no immediate switch of the running DMA.
 CHECK(!memcmp(cpu[b],ram[b].data(),Stride));
 CHECK(ram[b]==reference);chosen=b;published=reference;hasPublished=true;++submissions;
 return ESP_OK;
}
static size_t physical(unsigned x,unsigned y){
 // M5GFX0.2.30 _rotate_pixelcopy rotation 1: logical1280x720 ->720x1280.
 return x*Stride+(W-y-1)*2;
}
static void flush(lv_display_t* d,const lv_area_t* a,uint8_t* pixels){
 ++flushes;const int w=a->x2-a->x1+1,h=a->y2-a->y1+1;
 if(!transaction){drawable=tab5DisplayCacheBeginFrame();transaction=true;}
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){
  const auto off=physical(a->x1+x,a->y1+y);const auto at=(y*w+x)*2;
  memcpy(reference.data()+off,pixels+at,2);
  if(drawable){auto* back=static_cast<uint8_t*>(M5.Display.panel.config_detail().buffer);
   CHECK(back!=cpu[scanning]);memcpy(back+off,pixels+at,2);}
 }
 if(drawable){tab5DisplayCacheDirtyRect(a->x1,a->y1,w,h);Tab5DisplayActivity::flushed(w,h,lv_display_flush_is_last(d),clockMs);}
 if(lv_display_flush_is_last(d)){
  if(drawable){auto* back=static_cast<uint8_t*>(M5.Display.panel.config_detail().buffer);
   // M5GFX FrameBufferBase::display's cache request is conservatively allowed
   // to encompass all rows; production filtering must keep exact dirty bytes.
   Cache_WriteBack_Addr(uint32_t(uintptr_t(back)),Bytes);tab5DisplayCacheEndFrame();}
  transaction=false;++frames;
 }
 lv_display_flush_ready(d);
}
static void timers(){
 const auto before=clockMs;Tab5DisplayRefresh::service(lv_display_get_default());
 lv_timer_handler();CHECK(clockMs==before); // No display wait may hold the UI.
}
static void pump(unsigned duration){
 for(unsigned i=0;i<duration;i+=10){advance(10);timers();
  if(tab5DisplayCacheNeedsRedraw())lv_obj_invalidate(lv_screen_active());}
}
int main(){
 for(unsigned i=0;i<2;++i){cpu[i]=static_cast<uint8_t*>(mmap(nullptr,Bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0));CHECK(cpu[i]!=MAP_FAILED);memset(cpu[i],0,Bytes);ram[i].assign(Bytes,0);}
 for(unsigned y=0;y<H;++y)rows[y]=cpu[0]+y*Stride;
 M5.Display.panel.init(cpu[0],rows);M5.Display.panel.rotation=1;
 CHECK(tab5DisplayPresentInit());CHECK(tab5DisplayCacheInit());
 lv_init();lv_tick_set_cb([](){return clockMs;});auto* d=lv_display_create(1280,720);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 std::vector<uint8_t> draw(1280*32*2);lv_display_set_buffers(d,draw.data(),nullptr,draw.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);CHECK(Tab5DisplayActivity::begin(d));
 auto* screen=lv_screen_active();lv_obj_set_style_bg_color(screen,lv_color_hex(0x182430),0);lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
 auto* clock=lv_label_create(screen);lv_obj_set_pos(clock,1050,8);lv_label_set_text(clock,"12:00");
 auto* content=lv_obj_create(screen);lv_obj_set_pos(content,30,80);lv_obj_set_size(content,1160,570);lv_obj_set_style_bg_color(content,lv_color_hex(0x3a617b),0);
 auto* hidden=lv_obj_create(screen);lv_obj_set_size(hidden,1280,720);lv_obj_add_flag(hidden,LV_OBJ_FLAG_HIDDEN);auto* rtc=lv_label_create(hidden);
 pump(200);CHECK(submissions>0&&ram[scanning]==reference);
 // Hidden settings RTC text is updated once/second as in the OS. Idle refresh
 // requests/layout work must not publish, clear, copy, or repaint the screen.
 unsigned before=submissions,beforeSync=syncs;
 for(unsigned sec=0;sec<180;++sec){char s[32];snprintf(s,sizeof(s),"12:%02u:%02u",sec/60,sec%60);lv_label_set_text(rtc,s);pump(1000);}
 CHECK(submissions==before&&syncs==beforeSync);CHECK(ram[scanning]==reference);
 for(unsigned n=0;n<90;++n){char s[24];snprintf(s,sizeof(s),"12:%02u",n);lv_label_set_text(clock,s);pump(80);CHECK(ram[scanning]==reference);}
 // CPU busy / SD IO delay: a long gap between handlers does not publish a
 // transient cleared framebuffer. Real LVGL only emits required regions.
 advance(2750);lv_label_set_text(clock,"13:30");timers();pump(100);CHECK(ram[scanning]==reference);
 // Delayed DMA completion holds the LVGL refresh timer and its invalidations
 // until safe, without dropping a frame or changing the scanning bytes.
 lv_label_set_text(clock,"13:31");pump(40);paused=true;
 lv_label_set_text(clock,"13:32");pump(40);auto stable=ram[scanning];
 lv_label_set_text(clock,"13:33");pump(160);CHECK(ram[scanning]==stable);CHECK(tab5DisplayPresentStats().timeouts>0);
 paused=false;pump(300);CHECK(ram[scanning]==reference);
 // A failed writeback must not publish partially synchronized external RAM.
 cacheError=9;lv_obj_set_style_bg_color(content,lv_color_hex(0x467c42),0);pump(60);before=submissions;stable=ram[scanning];pump(300);CHECK(submissions==before&&ram[scanning]==stable);
 cacheError=0;pump(1300);CHECK(ram[scanning]==reference);CHECK(tab5DisplayCacheStats().recoveries>0);
 // Background allocations/memory churn must not create display work by itself.
 before=submissions;beforeSync=syncs;
 for(unsigned i=0;i<100;++i){std::vector<uint8_t> pressure(128*1024,uint8_t(i));pump(90);CHECK(pressure[i]==uint8_t(i));}
 CHECK(before==submissions&&beforeSync==syncs&&ram[scanning]==reference);
 printf("pipeline checks=%u publications=%u frames=%u flushes=%u scans=%u cache_calls=%u simulated_ms=%u timeouts=%u\n",checks,submissions,frames,flushes,scans,syncs,clockMs,tab5DisplayPresentStats().timeouts);
 Tab5DisplayRefresh::service(nullptr);lv_display_delete(d);lv_deinit();for(auto* p:cpu)munmap(p,Bytes);
}
