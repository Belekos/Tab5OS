#pragma once
#include <lvgl.h>
#include <stddef.h>
#include <Arduino.h>
#include "WriterKeys.h"
#include "Tab5FileSort.h"

// The catalogue remains in the audio engine. This model stores only references
// and renders 24 reusable rows regardless of the number of media items.
class Tab5MediaList {
 public:
  enum Kind : uint8_t { Media, Folder, Parent, Message, Import };
  struct Entry { Kind kind; uint32_t index; };
  using Format = void(*)(Entry, String&, const char*&, bool&, bool&, bool&);
  using Metadata = void(*)(Entry, String&, uint64_t&, int64_t&);
  using Action = void(*)(Entry, bool favorite);
  Tab5MediaList(lv_obj_t* list, const lv_font_t* font, Format format, Action action);
  ~Tab5MediaList();
  // Prepare a replacement model while the displayed model remains intact.
  void reset();
  void cancel();
  bool add(Kind kind, uint32_t index = 0);
  bool sort(Tab5FileSort::Order order, Metadata metadata);
  void show(int32_t scroll, uint32_t selected = UINT32_MAX);
  void select(uint32_t index);
  void appearance(const lv_font_t* font);
  bool key(const writing::KeyEvent& event);
  size_t count() const { return count_; }
  void release();
  static lv_obj_t* starButton(lv_obj_t* parent, int width, int height);
 private:
  static constexpr unsigned Pool = 24, Height = 74;
  struct Slot {
    Tab5MediaList* owner;
    lv_obj_t *row, *icon, *label, *favorite, *trash;
    size_t index = SIZE_MAX, pressed = SIZE_MAX;
    uint32_t generation = 0, formattedGeneration = UINT32_MAX;
  };
  lv_obj_t *list_, *extent_;
  const lv_font_t* font_;
  Format format_; Action action_;
  Entry* entries_ = nullptr;
  size_t count_ = 0, capacity_ = 0;
  Entry* pending_ = nullptr;
  size_t pendingCount_ = 0, pendingCapacity_ = 0;
  Slot slots_[Pool]{};
  size_t selected_ = SIZE_MAX;
  uint32_t generation_ = 0;
  bool rendering_ = false, rebuilding_ = false, appearanceSet_ = false;
  uint32_t colors_[9]{};
  uint8_t radius_ = 0;
  void render();
  void focus(size_t index, bool favorite = false);
  void activate(size_t index, bool favorite);
  static void rowEvent(lv_event_t* event);
  static void listEvent(lv_event_t* event);
};
