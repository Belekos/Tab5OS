#pragma once
#include <lvgl.h>
#include "WriterKeys.h"

namespace Tab5PythonReader {
struct Hooks {
  const char* (*translate)(const char*) = nullptr;
  const lv_font_t* (*font)(unsigned pixels) = nullptr;
  uint32_t (*color)(unsigned) = nullptr;
  // All hooks and public methods below run only on the UI task.
  void (*enterPortrait)() = nullptr;
  void (*leavePortrait)() = nullptr;
  void (*background)() = nullptr;
  const lv_font_t* (*uiFont)() = nullptr;
};
void begin(Hooks hooks);
void service(bool pythonForeground);
void suspend();
bool resume();
void close();
bool active();
bool visible();
bool key(const writing::KeyEvent& event);
}
