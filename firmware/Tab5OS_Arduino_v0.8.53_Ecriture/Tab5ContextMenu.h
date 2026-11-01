#pragma once
#include <lvgl.h>
#include <stddef.h>
#include "WriterKeys.h"

// Shared by mouse, keyboard and toolbar menus; all calls run on the UI task.
namespace Tab5ContextMenu {
constexpr size_t Capacity=32; // Explorer exposes all contextual actions, including native image handlers.
struct Item {
  const char* title;
  const char* shortcut;
  void (*action)(void*);
  void* user = nullptr;
  bool enabled = true;
};
void configure(const lv_font_t* (*font)(), const char* (*translate)(const char*));
// Labels are copied by LVGL. Item arrays may be local. The owner must outlive
// the action; deleting it cancels a queued action and closes the menu.
bool show(lv_obj_t* owner, int x, int y, const Item* items, size_t count);
void close();
void service();
bool key(const writing::KeyEvent& event);
lv_obj_t* scope();
// Standard editing menu for a textarea or explicitly selectable label.
bool text(lv_obj_t* hit, int x, int y);
// A console clears its backing model, so refresh cannot restore old output.
bool bindOutput(lv_obj_t* output, void (*clear)(void*), void* user = nullptr);
bool clearOutput(lv_obj_t* output);
}
