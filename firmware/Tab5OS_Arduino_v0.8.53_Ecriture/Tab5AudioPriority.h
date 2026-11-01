#pragma once
#include <atomic>
#include <stdint.h>

// One temporary priority owner. The generation travels with the lease so a
// timed-out or stale caller can never resume/release a later owner's audio.
class Tab5AudioPriorityControl {
 public:
  enum State : uint32_t { Idle, Requested, Preparing, Held, Release, Denied };
  uint32_t request() {
    uint32_t old = _value.load();
    if ((old & Mask) != Idle) return 0;
    uint32_t token = (old + Step) & ~Mask;
    if (!token) token = Step;
    return _value.compare_exchange_strong(old, token | Requested) ? token : 0;
  }
  uint32_t begin() {
    uint32_t old = _value.load();
    if ((old & Mask) != Requested) return 0;
    const uint32_t token = old & ~Mask;
    return _value.compare_exchange_strong(old, token | Preparing) ? token : 0;
  }
  void acknowledge(uint32_t token, bool accepted) {
    uint32_t old = token | Preparing;
    _value.compare_exchange_strong(old, token | (accepted ? Held : Denied));
  }
  State state(uint32_t token = 0) const {
    const uint32_t value = _value.load();
    return token && (value & ~Mask) != token ? Idle : State(value & Mask);
  }
  void release(uint32_t token) {
    uint32_t old = _value.load();
    while (token && (old & ~Mask) == token && (old & Mask) != Idle &&
           (old & Mask) != Release &&
           !_value.compare_exchange_weak(old, token | Release)) {}
  }
  uint32_t released() const {
    const uint32_t value = _value.load();
    return (value & Mask) == Release ? value & ~Mask : 0;
  }
  void complete(uint32_t token) {
    uint32_t old = token | Release;
    _value.compare_exchange_strong(old, token | Idle);
  }
 private:
  static constexpr uint32_t Mask = 7, Step = 8;
  std::atomic<uint32_t> _value{Idle};
};
