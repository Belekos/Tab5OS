#pragma once
#include <lvgl.h>
#include "WriterKeys.h"
namespace Tab5FilePicker {
using Result=void(*)(const char* path,void* context);
bool open(const char* directory,const char* suffix,bool folders,Result callback,void* context);
void close();
void service();
bool key(const writing::KeyEvent& event);
bool visible();
lv_obj_t* scope();
}
