#pragma once
#include <lvgl.h>
#include "WriterKeys.h"
namespace Tab5Alarm {
struct Hooks {
 const char* (*translate)(const char*)=nullptr;
 const lv_font_t* (*font)()=nullptr;
 uint32_t (*color)(unsigned)=nullptr;
 void (*wake)()=nullptr;
 void (*quiet)()=nullptr;
 // Saves UI-owned state, returning a translated-at-display error key on failure.
 const char* (*prepareSleep)()=nullptr;
};
void begin(Hooks);
void open();
void service();
bool visible();
bool ringing();
void key(const writing::KeyEvent&);
}
