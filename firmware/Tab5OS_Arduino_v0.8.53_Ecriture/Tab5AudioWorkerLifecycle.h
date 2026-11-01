#pragma once
#include <atomic>
#include <stdint.h>

// A single starter owns Stopped -> Starting -> Running. Only the worker may
// publish Running -> Stopped, after its final access to ordinary engine state.
// No task handle is dereferenced after retirement; FreeRTOS owns final cleanup.
class Tab5AudioWorkerLifecycle {
 public:
  bool tryStart() {
    uint8_t expected = Stopped;
    return _state.compare_exchange_strong(expected, Starting, std::memory_order_acq_rel);
  }
  void started() { _state.store(Running, std::memory_order_release); }
  void failedStart() { _state.store(Stopped, std::memory_order_release); }
  bool starting() const { return _state.load(std::memory_order_acquire) == Starting; }
  bool active() const { return _state.load(std::memory_order_acquire) != Stopped; }
  void stopped() { _state.store(Stopped, std::memory_order_release); }
 private:
  enum : uint8_t { Stopped, Starting, Running };
  std::atomic<uint8_t> _state{Stopped};
};
