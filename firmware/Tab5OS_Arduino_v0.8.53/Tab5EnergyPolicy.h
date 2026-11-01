#pragma once
#include <stdint.h>

// Backlight only: no deep sleep, CPU clock change, lost network session or
// discarded unsaved document. The first touch after darkness only wakes.
class Tab5ScreenIdle {
 public:
  enum class Level : uint8_t { Awake, Dim, Off };
  bool touch(bool pressed, uint32_t now) {
    if (pressed) {
      if (_level == Level::Off) _consumeTouch = true;
      _activity = now; _level = Level::Awake;
    } else { _consumeTouch = false; }
    return _consumeTouch;
  }
  Level update(uint32_t now, uint32_t timeoutMs, bool keepAwake) {
    if (keepAwake) _activity = now;
    const uint32_t idle = now - _activity;
    const uint32_t dimAt = timeoutMs / 3U < 60000U ? timeoutMs / 3U : 60000U;
    _level = !timeoutMs || keepAwake ? Level::Awake :
        (idle >= timeoutMs ? Level::Off : (idle >= dimAt ? Level::Dim : Level::Awake));
    return _level;
  }
  void wake(uint32_t now) { _activity = now; _level = Level::Awake; }
  Level level() const { return _level; }
 private:
  uint32_t _activity = 0;
  Level _level = Level::Awake;
  bool _consumeTouch = false;
};
