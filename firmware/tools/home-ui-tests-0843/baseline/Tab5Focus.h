#pragma once
#include <lvgl.h>

#include <vector>

#include "WriterKeys.h"
class Tab5FocusClass {
 public:
  void begin(lv_obj_t* screen, const lv_font_t* font);
  // Use the OS palette for button focus and shortcut tips. Text surfaces keep
  // their normal appearance, including transparent backgrounds.
  void theme(lv_color_t selected, lv_color_t text, lv_color_t surface);
  void describe(lv_obj_t* object, const char* text);
  void clear();
  void focusFirst(lv_obj_t* scope);
  // Opening a dialog starts at its input/action, not its decoration or close
  // button. Call after the controls have been constructed.
  void focusInitial(lv_obj_t* scope, lv_obj_t* primary = nullptr);
  void focusObject(lv_obj_t* object);
  void editObject(lv_obj_t* textarea);
  // Keep the OS touch keyboard on the same input as physical Tab navigation.
  void inputChanged(void (*callback)(lv_obj_t*)) { inputChanged_ = callback; }
  void finishEditing() { controlEditing_ = false; }
  void remember(lv_obj_t* scope);
  void restore(lv_obj_t* scope);
  bool contains(lv_obj_t* scope, lv_obj_t* object) const;
  bool controlling() const { return controlEditing_; }
  // Lists can select on focus and reserve Enter for an explicit action.
  void activation(lv_obj_t* object, lv_event_cb_t callback, void* data = nullptr);
  bool key(const writing::KeyEvent& event, lv_obj_t* scope, lv_obj_t* exclude = nullptr,
           lv_obj_t* neighbor = nullptr);
  bool edit(lv_obj_t* textarea, const writing::KeyEvent& event);
  lv_obj_t* focused() const;

 private:
  lv_obj_t *current_ = nullptr, *tip_ = nullptr;
  bool controlEditing_ = false;
  void (*inputChanged_)(lv_obj_t*) = nullptr;
  lv_color_t selected_ = lv_color_hex(0x30363d), text_ = lv_color_hex(0xf1f3f5),
             surface_ = lv_color_hex(0x191c20);
  lv_timer_t* tipTimer_ = nullptr;
  static void tipTimer(lv_timer_t* timer);
  struct Memory {
    lv_obj_t* scope;
    lv_obj_t* object;
  };
  std::vector<Memory> memories_;
  static void deleted(lv_event_t* event);
  void watch(lv_obj_t* object);
  void focus(lv_obj_t* object);
  void applyFocusStyle(lv_obj_t* object);
};
extern Tab5FocusClass Tab5Focus;
