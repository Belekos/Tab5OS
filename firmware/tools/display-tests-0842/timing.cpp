// Same CPU-cache/PSRAM/DMA fixtures as the complete pixel-safety suite.
// The baseline links the exact released .41 files; only hardware is modelled.
#define main display_safety_main
#include "pipeline.cpp"
#undef main
#ifdef TAB5_DISPLAY_BASELINE
namespace Tab5DisplayRefresh {void service(lv_display_t*) {}}
#endif
static unsigned serviceCalls=0;
static bool timerChanges=true;
int main(){
 for(unsigned i=0;i<2;++i){cpu[i]=static_cast<uint8_t*>(mmap(nullptr,Bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0));CHECK(cpu[i]!=MAP_FAILED);memset(cpu[i],0,Bytes);ram[i].assign(Bytes,0);}
 for(unsigned y=0;y<H;++y)rows[y]=cpu[0]+y*Stride;
 M5.Display.panel.init(cpu[0],rows);M5.Display.panel.rotation=1;
 CHECK(tab5DisplayPresentInit());CHECK(tab5DisplayCacheInit());
 lv_init();
#ifndef TAB5_DISPLAY_BASELINE
 lv_tick_set_cb([](){return clockMs;});
#endif
 auto* d=lv_display_create(1280,720);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 lv_timer_set_period(lv_display_get_refr_timer(d),16); // Production lv_conf.h.
 std::vector<uint8_t> draw(1280*32*2);lv_display_set_buffers(d,draw.data(),nullptr,draw.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 auto* label=lv_label_create(lv_screen_active());lv_obj_set_pos(label,8,8);
 lv_timer_create([](lv_timer_t* timer){++serviceCalls;if(timerChanges)lv_obj_set_x(static_cast<lv_obj_t*>(lv_timer_get_user_data(timer)),8+serviceCalls%2);},7,label);
 unsigned lastLoop=0,lastInput=0,maxBlock=0,totalBlocked=0,maxInputGap=0,maxTickLag=0;
 auto step=[&](unsigned n){advance(8);auto start=clockMs;maxInputGap=std::max(maxInputGap,start-lastInput);lastInput=start;
  char text[24];snprintf(text,sizeof(text),"%u",n);lv_label_set_text(label,text);
#ifdef TAB5_DISPLAY_BASELINE
  lv_tick_inc(start-lastLoop);
#endif
  lastLoop=start;Tab5DisplayRefresh::service(d);lv_timer_handler();
  maxBlock=std::max(maxBlock,clockMs-start);totalBlocked+=clockMs-start;maxTickLag=std::max(maxTickLag,clockMs-lv_tick_get());
  if(tab5DisplayCacheNeedsRedraw())lv_obj_invalidate(lv_screen_active());
 };
 for(unsigned n=0;clockMs<1000;++n)step(n);
 auto normalBlock=maxBlock,normalInput=maxInputGap,normalTick=maxTickLag,normalService=serviceCalls;
 paused=true;auto stop=clockMs+320;maxBlock=maxInputGap=maxTickLag=0;
 for(unsigned n=10000;clockMs<stop;++n)step(n);
 const auto pausedBlock=maxBlock,pausedInput=maxInputGap,pausedTick=maxTickLag,pausedServices=serviceCalls-normalService;
 paused=false;for(unsigned n=20000;n<20040;++n)step(n);
 // A separate LVGL timer invalidates during the handler, after the outer gate.
 // Stop producing new UI changes: the last dirty region must still be rendered.
 timerChanges=false;
 for(unsigned n=0;n<20;++n){advance(8);
#ifdef TAB5_DISPLAY_BASELINE
  lv_tick_inc(clockMs-lastLoop);
#endif
  lastLoop=clockMs;Tab5DisplayRefresh::service(d);lv_timer_handler();if(tab5DisplayCacheNeedsRedraw())lv_obj_invalidate(lv_screen_active());}
 CHECK(ram[scanning]==reference);
#ifdef TAB5_DISPLAY_BASELINE
 CHECK(normalBlock>0&&pausedBlock==100&&normalTick>0&&pausedTick==100);
#else
 CHECK(normalBlock==0&&pausedBlock==0&&normalInput==8&&pausedInput==8&&normalTick==0&&pausedTick==0);
 CHECK(pausedServices>=39);CHECK(tab5DisplayPresentStats().skippedFrames==0);
#endif
 printf("timing normal_max_block_ms=%u normal_max_input_gap_ms=%u normal_tick_lag_ms=%u paused_max_block_ms=%u paused_max_input_gap_ms=%u paused_tick_lag_ms=%u paused_service_calls=%u total_blocked_ms=%u skipped=%u checks=%u\n",normalBlock,normalInput,normalTick,pausedBlock,pausedInput,pausedTick,pausedServices,totalBlocked,tab5DisplayPresentStats().skippedFrames,checks);
 Tab5DisplayRefresh::service(nullptr);lv_display_delete(d);lv_deinit();for(auto* p:cpu)munmap(p,Bytes);
}
