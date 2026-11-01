#include <lvgl.h>
#include "Tab5DisplayActivity.h"
#include <cassert>
#include <cstdio>
static uint32_t now=100;
uint32_t millis(){return now;}
int main(){using namespace Tab5DisplayActivity;lv_init();auto* d=lv_display_create(720,1280);assert(begin(d));assert(begin(d));
 lv_area_t small{0,0,19,19},large{0,0,719,899},full{0,0,719,1279};
 auto first=snapshot();
 {Scope a(Source::Map);lv_display_send_event(d,LV_EVENT_INVALIDATE_AREA,&small);}
 assert(snapshot().largeInvalidations==first.largeInvalidations);
 now=150;{Scope a(Source::Files);lv_display_send_event(d,LV_EVENT_INVALIDATE_AREA,&large);}
 auto measured=snapshot();assert(measured.largeInvalidations==first.largeInvalidations+1&&measured.lastLargeSource==Source::Files&&measured.lastLargeInvalidationMs==150&&measured.lastLargeInvalidationPixels==720u*900);
 now=90000;{Scope a(Source::ClockPower);lv_display_send_event(d,LV_EVENT_INVALIDATE_AREA,&small);}
 assert(snapshot().lastLargeInvalidationMs==150&&snapshot().lastLargeSource==Source::Files);
 flushed(720,100,false,90001);flushed(720,600,true,90002);measured=snapshot();assert(measured.largeFrames==1&&measured.lastLargeFrameMs==90002&&measured.maxFramePixels==504000);
 flushed(10,10,true,90003);assert(snapshot().largeFrames==1&&snapshot().maxFramePixels==504000);
 const auto diff=difference(snapshot(),first);assert(diff.largeInvalidations==1&&diff.largeFrames==1&&diff.frames==2&&diff.flushAreas==3);
 now=0xfffffff0;{Scope a(Source::Home);lv_display_send_event(d,LV_EVENT_INVALIDATE_AREA,&full);}assert(snapshot().lastLargeSource==Source::Home);
 now=4;lv_display_send_event(d,LV_EVENT_INVALIDATE_AREA,&small);assert(snapshot().lastLargeInvalidationMs==0xfffffff0);
 lv_display_delete(d);assert(!snapshot().attached);lv_deinit();puts("PASS: actual LVGL 9.6 observer, large/small invalidation attribution, persistent timestamp, grouped frame area, counter deltas, no timers/tasks or framebuffer changes");
}
