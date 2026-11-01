#pragma once
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>
#include "WriterKeys.h"
#include "Tab5AppApi.h"
namespace Tab5NativeView {
void hooks(const char* (*translate)(const char*),const lv_font_t* (*font)(),uint8_t (*volume)(),void (*setVolume)(uint8_t));
void extendedControls(bool enabled);
void key(const writing::KeyEvent&);
bool begin(unsigned width,unsigned height); // application worker
bool canvasBegin(unsigned width,unsigned height);
int inputPoll(tab5_app_input_event* event);
bool present(const uint16_t*,size_t pixels); // application worker, copies
uint32_t buttons();
void service(); // UI task
void close(); // UI task, only after worker deletion
bool visible();
}
