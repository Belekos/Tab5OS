#pragma once
struct _lv_display_t;
namespace Tab5DisplayRefresh {
// Call on the UI task before lv_timer_handler(). Only the display refresh timer
// is held while its framebuffer is unavailable; input and other timers continue.
void service(_lv_display_t* display);
}
