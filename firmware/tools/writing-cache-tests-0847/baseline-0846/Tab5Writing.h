#pragma once
#include <lvgl.h>

#include "WriterKeys.h"

namespace writing { struct ViewMetrics; }

enum class WritingColor : uint8_t { Background, Surface, Text, Muted, Accent, Selection, Reference };

struct WritingHooks {
  void (*keyboard)(bool, lv_obj_t*) = nullptr;
  bool (*keyboardOnFocus)() = nullptr;
  void (*notice)(const char*) = nullptr;
  const lv_font_t* (*font)(uint16_t, uint8_t) = nullptr;
  bool (*imageLoad)(const char*, int, int, lv_image_dsc_t*, void**) = nullptr;
  void (*imageFree)(void*) = nullptr;
  const char* (*translate)(const char*) = nullptr;
  uint32_t (*color)(WritingColor) = nullptr;
  void (*help)() = nullptr;
  void (*print)(const char*) = nullptr;
  bool (*run)(const char* savedPath) = nullptr;
  void (*stop)() = nullptr;
  void (*resume)() = nullptr;
  void (*libraries)() = nullptr;
  size_t (*console)(char* destination, size_t capacity) = nullptr;
  void (*clearConsole)() = nullptr;
  bool (*running)() = nullptr;
};
class Tab5WritingApp {
 public:
  void begin(lv_obj_t* parent, WritingHooks hooks);
  void show(bool visible);
  void refreshAppearance();
  // Text in the document and its reference pane; toolbar sizes are independent.
  // Repeating the same setting preserves the editor's local +/- zoom.
  void setTextSize(uint16_t pixels);
  const writing::ViewMetrics* viewMetrics(bool reference = false) const;
  // True also when an encoding chooser is waiting for the user.
  bool open(const char* path);
  bool create(const char* directory = "/");
  bool createCode(const char* directory = "/MicroPython");
  static bool codeFile(const char* path);
  const char* path() const;
  bool development() const;
  void resize(int height);
  void service(uint32_t now, bool autosave, uint32_t delayMs);
  bool key(const writing::KeyEvent& event, lv_obj_t* neighbor = nullptr);
  bool contextMenu(lv_obj_t* hit,int x,int y);
  lv_obj_t* object() const;
  bool isInput(lv_obj_t* target) const;
  bool editing() const;
  bool storageIdle() const;
  bool prepareSleep();
  size_t reclaimHiddenLayout();
  lv_obj_t* navigationScope() const;
  void leaveEditing();
  void insert(const char* text);
  void touchKey(writing::Key key);

 private:
  struct Impl;
  Impl* impl_ = nullptr;
  uint16_t textSize_ = 24;
};
extern Tab5WritingApp Tab5Writing;
