#include "platform.h"
#include "Tab5DisplayPresent.h"
#include "Tab5DisplayCache.h"
#include "Tab5DisplayCacheRecovery.h"
#include "Tab5DisplayCacheRanges.h"
#include "Tab5DisplayPresentFence.h"
#include <sys/mman.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>
M5Mock M5;
void tab5DisplayBandwidthInit(){}
int Cache_WriteBack_Addr(uint32_t,uint32_t);
static constexpr size_t Bytes=720*1280*2;
static uint8_t* fb[2]{};static uint8_t* rows[1280];
static const void* selected;static void* task=reinterpret_cast<void*>(1);
static uint32_t clockMs;static int cacheError,publishError;static unsigned syncCalls,publishCalls,checks,autoTicks;
static esp_lcd_dpi_panel_event_callbacks_t callback;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"check %u line %d: %s\n",checks,__LINE__,#x);abort();}}while(0)
TaskHandle_t xTaskGetCurrentTaskHandle(){return task;}
static void refresh(){CHECK(callback.on_refresh_done);callback.on_refresh_done(nullptr,nullptr,nullptr);}
void vTaskDelay(unsigned ticks){clockMs+=ticks;if(autoTicks) {--autoTicks;refresh();}}
int64_t esp_timer_get_time(){return int64_t(clockMs)*1000;}
bool esp_ptr_external_ram(const void*){return true;}
void* heap_caps_malloc(size_t n,int){return malloc(n);}void heap_caps_free(void* p){free(p);}
int esp_cache_msync(void*,size_t n,int flags){CHECK(n<=16384);CHECK(flags==(ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_TYPE_DATA));++syncCalls;return cacheError;}
int esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t,unsigned n,void** a,void** b){CHECK(n==2);*a=fb[0];*b=fb[1];return ESP_OK;}
int esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t,const esp_lcd_dpi_panel_event_callbacks_t* c,void*){callback=*c;return ESP_OK;}
int esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t,int x,int y,int w,int h,const void* data){CHECK(x==0&&y==0&&w==720&&h==1);++publishCalls;if(publishError)return publishError;selected=data;return ESP_OK;}
static void paint(unsigned x,unsigned y,unsigned w,unsigned h,uint8_t value){
 tab5DisplayPresentDirtyRect(x,y,w,h);
 for(unsigned row=y;row<y+h;++row)memset(M5.Display.panel.row(row)+x*2,value,w*2);
}
static void finish(){refresh();refresh();}
int main(){
 for(auto& p:fb){p=(uint8_t*)mmap(nullptr,Bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);CHECK(p!=MAP_FAILED);CHECK((uintptr_t(p)&127)==0);}
 memset(fb[0],0x11,Bytes);memset(fb[1],0xCC,Bytes);
 for(unsigned y=0;y<1280;++y)rows[y]=fb[0]+y*1440;
 selected=fb[0];M5.Display.panel.init(fb[0],rows);
 CHECK(tab5DisplayPresentInit());CHECK(tab5DisplayPresentStats().active);
 CHECK(tab5DisplayPresentBeginFrame());CHECK(memcmp(fb[0],fb[1],Bytes)==0);
 paint(10,20,8,6,0x22);CHECK(fb[0][20*1440+20]==0x11);
 tab5DisplayPresentEndFrame(true);CHECK(selected==fb[1]);CHECK(tab5DisplayPresentStats().pending);
 // One completion is never enough. Missing second completion drops the complete
 // frame; it never switches to writing the framebuffer still being scanned.
 refresh();auto before=std::vector<uint8_t>(fb[0],fb[0]+Bytes);
 CHECK(!tab5DisplayPresentBeginFrame());CHECK(clockMs==0);clockMs+=100;CHECK(!tab5DisplayPresentReady());CHECK(tab5DisplayPresentStats().timeouts==1);CHECK(memcmp(before.data(),fb[0],Bytes)==0);
 CHECK(!tab5DisplayPresentNeedsRedraw());refresh();CHECK(tab5DisplayPresentNeedsRedraw());
 CHECK(tab5DisplayPresentBeginFrame());CHECK(M5.Display.panel.config_detail().buffer==fb[0]);CHECK(memcmp(fb[0],fb[1],Bytes)==0);
 paint(40,50,3,4,0x33);tab5DisplayPresentEndFrame(true);finish();
 // Consecutive partial frames must preserve all earlier regions, not alternate
 // a current/stale image as the two buffers exchange roles.
 std::vector<uint8_t> expected(fb[0],fb[0]+Bytes);
 for(unsigned n=0;n<250;++n){
   CHECK(tab5DisplayPresentBeginFrame());const auto s=tab5DisplayPresentStats();
   CHECK(memcmp(fb[s.front],expected.data(),Bytes)==0);CHECK(memcmp(fb[s.back],expected.data(),Bytes)==0);
   unsigned x=(n*17)%680,y=(n*23)%1240,w=1+n%25,h=1+n%19;uint8_t v=uint8_t(n+1);
   paint(x,y,w,h,v);for(unsigned r=y;r<y+h;++r)memset(expected.data()+r*1440+x*2,v,w*2);
   CHECK(memcmp(fb[s.front],expected.data(),Bytes)!=0 || v==0x11);
   tab5DisplayPresentEndFrame(true);CHECK(memcmp(selected,expected.data(),Bytes)==0);finish();
 }
 CHECK(tab5DisplayPresentBeginFrame());auto old=tab5DisplayPresentStats();
 auto snapshot=std::vector<uint8_t>(fb[old.front],fb[old.front]+Bytes);
 paint(0,0,20,20,0xEE);tab5DisplayPresentEndFrame(false);
 unsigned submits=publishCalls;CHECK(memcmp(fb[old.front],snapshot.data(),Bytes)==0);
 CHECK(!tab5DisplayPresentBeginFrame());CHECK(!tab5DisplayPresentNeedsRedraw());
 clockMs+=1000;CHECK(tab5DisplayPresentNeedsRedraw());CHECK(tab5DisplayPresentBeginFrame());
 paint(0,0,720,1280,0x44);tab5DisplayPresentEndFrame(true);CHECK(publishCalls==submits+1);finish();
 // Publish error never switches visible image, retry is paced, repaint follows.
 CHECK(tab5DisplayPresentBeginFrame());old=tab5DisplayPresentStats();snapshot.assign(fb[old.front],fb[old.front]+Bytes);
 publishError=7;paint(3,5,20,20,0x77);tab5DisplayPresentEndFrame(true);
 CHECK(memcmp(fb[old.front],snapshot.data(),Bytes)==0);CHECK(tab5DisplayPresentStats().publishErrors==1);
 CHECK(!tab5DisplayPresentNeedsRedraw());clockMs+=1000;CHECK(tab5DisplayPresentNeedsRedraw());publishError=0;
 CHECK(tab5DisplayPresentBeginFrame());paint(0,0,720,1280,0x55);tab5DisplayPresentEndFrame(true);finish();
 // Cache mirror failure leaves scanning pixels intact, retries no more often
 // than error backoff and does not blank either frame.
 cacheError=8;CHECK(!tab5DisplayPresentBeginFrame());old=tab5DisplayPresentStats();
 CHECK(fb[old.front][0]==0x55);unsigned calls=syncCalls;CHECK(!tab5DisplayPresentBeginFrame());CHECK(syncCalls==calls);
 clockMs+=1000;cacheError=0;CHECK(tab5DisplayPresentNeedsRedraw());CHECK(tab5DisplayPresentBeginFrame());paint(0,0,720,1280,0x66);tab5DisplayPresentEndFrame(true);finish();
 // Long inactivity triggers no copies, publishes, invalidations, or clearing.
 calls=syncCalls;submits=publishCalls;
 for(unsigned n=0;n<1000;++n){clockMs+=30000;CHECK(!tab5DisplayPresentNeedsRedraw());}
 CHECK(syncCalls==calls);CHECK(publishCalls==submits);CHECK(((const uint8_t*)selected)[0]==0x66);
 task=reinterpret_cast<void*>(2);CHECK(!tab5DisplayPresentBeginFrame());CHECK(!tab5DisplayPresentNeedsRedraw());task=reinterpret_cast<void*>(1);
 // Fence arithmetic survives counter wrapping.
 CHECK(!Tab5DisplayPresentFence::released(UINT32_MAX,UINT32_MAX));CHECK(!Tab5DisplayPresentFence::released(0,UINT32_MAX));CHECK(Tab5DisplayPresentFence::released(1,UINT32_MAX));
 // Error retries are dormant until an explicit cache failure and wrap safely.
 Tab5DisplayCacheRecovery::Retry retry;CHECK(!retry.requestRedraw(90000));retry.failed(UINT32_MAX-500);CHECK(!retry.requestRedraw(498));CHECK(retry.requestRedraw(499));CHECK(!retry.requestRedraw(499));CHECK(retry.beginAttempt(499));retry.recovered();CHECK(!retry.pending());
 // Exercise the actual cache filter with the present state machine, not a
 // reimplementation: exact dirty ranges, fallback and delayed recovery.
 CHECK(tab5DisplayCacheInit());
 CHECK(tab5DisplayCacheBeginFrame());
 tab5DisplayCacheDirtyRect(3,7,11,13);
 for(unsigned row=7;row<20;++row)memset(M5.Display.panel.row(row)+6,0x91,22);
 auto* draw=(uint8_t*)M5.Display.panel.config_detail().buffer;
 CHECK(Cache_WriteBack_Addr(uint32_t(uintptr_t(draw+7*1440+6)),12*1440+22)==ESP_OK);
 tab5DisplayCacheEndFrame();CHECK(tab5DisplayCacheStats().filteredFrames==1);
 finish();CHECK(!tab5DisplayCacheNeedsRedraw());
 CHECK(tab5DisplayCacheBeginFrame());tab5DisplayCacheDirtyRect(0,0,10,10);
 cacheError=9;memset(M5.Display.panel.row(0),0xA1,20);
 draw=(uint8_t*)M5.Display.panel.config_detail().buffer;
 (void)Cache_WriteBack_Addr(uint32_t(uintptr_t(draw)),10*1440);
 submits=publishCalls;tab5DisplayCacheEndFrame();CHECK(publishCalls==submits);
 CHECK(tab5DisplayCacheStats().pendingRecovery);
 CHECK(!tab5DisplayCacheNeedsRedraw());clockMs+=999;CHECK(!tab5DisplayCacheNeedsRedraw());++clockMs;
 CHECK(tab5DisplayCacheNeedsRedraw());cacheError=0;
 CHECK(tab5DisplayCacheBeginFrame());tab5DisplayCacheDirtyRect(0,0,720,1280);
 for(unsigned row=0;row<1280;++row)memset(M5.Display.panel.row(row),0xB1,1440);
 draw=(uint8_t*)M5.Display.panel.config_detail().buffer;
 CHECK(Cache_WriteBack_Addr(uint32_t(uintptr_t(draw)),Bytes)==ESP_OK);
 tab5DisplayCacheEndFrame();CHECK(!tab5DisplayCacheStats().pendingRecovery);finish();
 CHECK(!tab5DisplayCacheNeedsRedraw());CHECK(tab5DisplayCacheStats().recoveries==1);
 for(unsigned n=0;n<100;++n){clockMs+=30000;CHECK(!tab5DisplayCacheNeedsRedraw());}
 const auto s=tab5DisplayPresentStats();printf("display checks=%u frames=%u flips=%u timeouts=%u publish_errors=%u copy_errors=%u\n",checks,s.submissions,s.flips,s.timeouts,s.publishErrors,s.copyErrors);
 for(auto p:fb)munmap(p,Bytes);
}
