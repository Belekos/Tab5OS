#pragma once
#include <lvgl.h>
#include "WriterKeys.h"
namespace Tab5ImageEditor {
struct Hooks {
 const char* (*translate)(const char*)=nullptr;
 const lv_font_t* (*font)()=nullptr;
 void (*keyboard)(bool,lv_obj_t*)=nullptr;
 void (*closed)()=nullptr;
};
void begin(lv_obj_t* parent,Hooks hooks);
bool open(const char* path=nullptr);
// Backgrounding retains the document; only explicit close releases it.
void show(bool visible);
bool close(bool discard=false);
void service();
void resize(int width,int height);
bool key(const writing::KeyEvent& event);
// Logical button bits after OS handedness: primary 1, secondary 2, middle 4.
// Secondary uses the second colour through the active tool; middle pans.
// The app consumes pointer reports over its uncovered canvas, including hover.
bool mouse(int screenX,int screenY,uint8_t buttons,int wheel);
// Cancel a pointer capture when another OS dialog takes ownership.
void cancelPointer();
lv_obj_t* scope();
lv_obj_t* modalScope();
bool visible();
bool busy();
bool dirty();
}
