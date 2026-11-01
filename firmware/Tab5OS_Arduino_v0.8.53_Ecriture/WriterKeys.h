#pragma once
#include <stdint.h>
#include <stddef.h>
namespace writing {
enum class Key : uint8_t {
  None,
  Text,
  Enter,
  Tab,
  Escape,
  Backspace,
  Delete,
  Insert,
  Left,
  Right,
  Up,
  Down,
  Home,
  End,
  PageUp,
  PageDown,
  F6,
  F1,
  Command,
  F5,
  ContextMenu,
  F2
};
struct KeyEvent {
  Key key = Key::None;
  char text[12] = {};
  bool ctrl = false, shift = false, alt = false, meta = false;
};
inline bool repeatable(const KeyEvent& e) {
  if (e.ctrl || e.alt || e.meta) return false;
  switch (e.key) {
    case Key::Text:
    case Key::Tab:
    case Key::Backspace:
    case Key::Delete:
    case Key::Left:
    case Key::Right:
    case Key::Up:
    case Key::Down:
    case Key::Home:
    case Key::End:
    case Key::PageUp:
    case Key::PageDown:
      return true;
    default:
      return false;
  }
}
class CommandLatch {
 public:
  bool report(uint8_t modifiers, const uint8_t* keys) {
    bool now = (modifiers & 0x88) != 0;
    if (now && !held_) alone_ = true;
    for (unsigned i = 0; i < 6; ++i)
      if (keys[i]) alone_ = false;
    bool result = held_ && !now && alone_;
    held_ = now;
    if (!now) alone_ = false;
    return result;
  }
  void reset() { held_ = alone_ = false; }

 private:
  bool held_ = false, alone_ = false;
};
struct KeyMapping {uint8_t usage=0,dead=0;bool letter=false;char values[3][8]{};};
class KeyDecoder {
 public:
  void french(bool value) {
    french_ = value;mapping_=nullptr;mappingCount_=0;
    dead_ = 0;
  }
  void mapping(const KeyMapping* data,size_t size){mapping_=data;mappingCount_=size;dead_=0;}
  bool isFrench() const { return french_; }
  void reset() { dead_ = 0; }
  KeyEvent press(uint8_t usage, uint8_t modifiers);

 private:
  const KeyMapping* mapping_=nullptr;size_t mappingCount_=0;
  bool french_ = true, caps_ = false;
  char dead_ = 0;
};
}  // namespace writing
