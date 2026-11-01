#include "Tab5DisplayRefresh.h"
#include "Tab5DisplayPresent.h"
#include <lvgl.h>
namespace Tab5DisplayRefresh {
namespace {
lv_timer_t* attached=nullptr;
bool held=false;
void refresh(lv_timer_t* timer) {
  // LVGL resumes this timer whenever an object invalidates. Check here, before
  // LVGL consumes those regions, even if another timer just invalidated an object.
  if (!tab5DisplayPresentReady()) {
    lv_timer_pause(timer);
    held=true;
    return;
  }
  held=false;
  lv_display_refr_timer(timer);
}
}
void service(lv_display_t* display) {
  if (!display) { attached=nullptr; held=false; return; }
  auto* timer=lv_display_get_refr_timer(display);
  if (attached!=timer) {
    attached=timer;
    held=false;
    // Standard LVGL driver hook: retain the existing timer and its user data.
    if (timer) lv_timer_set_cb(timer,refresh);
  }
  const bool ready=tab5DisplayPresentReady();
  if (timer && held && ready) {
    held=false;
    lv_timer_resume(timer);
    lv_timer_ready(timer);
  }
}
}
