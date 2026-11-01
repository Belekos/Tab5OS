#pragma once
#include <stdint.h>

namespace Tab5DisplayCacheRecovery {
class Retry {
 public:
  void failed(uint32_t now) {
    if (pending_) return;
    pending_ = true;
    next_ = now + kInterval;
  }
  bool requestRedraw(uint32_t now) {
    if (!due(now) || queued_) return false;
    queued_ = true;
    return true;
  }
  bool beginAttempt(uint32_t now) {
    if (!due(now)) return false;
    queued_ = false;
    next_ = now + kInterval;
    return true;
  }
  void recovered() { pending_ = queued_ = false; }
  bool pending() const { return pending_; }

 private:
  bool due(uint32_t now) const {
    return pending_ && static_cast<int32_t>(now - next_) >= 0;
  }
  static constexpr uint32_t kInterval = 1000;
  uint32_t next_ = 0;
  bool pending_ = false;
  bool queued_ = false;
};
}
