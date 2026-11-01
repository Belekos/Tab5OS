#pragma once
#include "Tab5Tools.h"
namespace Tab5SdrUi {
bool open(lv_obj_t* parent,const Tab5ToolsHooks&);
void close();void service();
bool key(const writing::KeyEvent&);
bool isInput(lv_obj_t*);
void insert(const char*);void touchKey(writing::Key);
lv_obj_t* input();
}
