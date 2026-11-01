#pragma once
#include <stdint.h>

// A continuously supplied stream must still let FreeRTOS idle/network run.
// Unsigned subtraction keeps this correct across millis() rollover.
class Tab5TimeSlice {
 public:
  bool due(uint32_t now, uint32_t budgetMs = 4) {
    if (uint32_t(now - last_) < budgetMs) return false;
    last_ = now;
    return true;
  }
 private:
  uint32_t last_ = 0;
};
