#pragma once
#include <lvgl.h>
#include "WriterKeys.h"
namespace Tab5UsbFormatUi {
struct Hooks {
  const char* (*translate)(const char*)=nullptr;
  const lv_font_t* (*font)()=nullptr;
  uint32_t (*color)(unsigned)=nullptr;
};
bool open(const Hooks& hooks);
void close();
void service();
lv_obj_t* scope();
bool key(const writing::KeyEvent& event);
}
