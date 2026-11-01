#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef ARDUINO
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include "Tab5Cooperate.h"

// Transport is NetworkClient or NetworkClientSecure. All methods and the
// cancellation callback run on the connection's owning task.
template<class Transport>
class Tab5CooperativeClient : public Transport {
 public:
  using Cancel = bool (*)(void*);
  explicit Tab5CooperativeClient(Cancel cancelled = nullptr,
                                 void* context = nullptr)
      : _cancelled(cancelled), _context(context) {}

  // A reusable connection can serve several requests. Bind only on its owning
  // task and clear before the request's callback context goes out of scope.
  // A cancelled transport has already stopped; HTTPClient will reconnect it.
  void setCancellation(Cancel cancelled, void* context) {
    _cancelled = cancelled;
    _context = context;
    _interrupted = false;
  }
  void clearCancellation() {
    _cancelled = nullptr;
    _context = nullptr;
  }

  int read(uint8_t* data, size_t size) override {
    return ready() ? Transport::read(data, size) : -1;
  }
  int read() override {
    uint8_t byte;
    return read(&byte, 1) == 1 ? byte : -1;
  }
  int available() override {
    return ready() ? Transport::available() : 0;
  }
  uint8_t connected() override {
    return ready() ? Transport::connected() : 0;
  }
  bool interrupted() const { return _interrupted; }

 private:
  bool ready() {
    // Stream::timedRead() busy-polls read() for a fragmented HTTP header.
    // delay(0) cannot run a lower-priority idle task; block for a real tick.
    if (_slice.due(millis())) vTaskDelay(1);
    if (!_interrupted && _cancelled && _cancelled(_context)) {
      _interrupted = true;
      this->setTimeout(0);
      Transport::stop();
    }
    return !_interrupted;
  }
  Cancel _cancelled;
  void* _context;
  Tab5TimeSlice _slice;
  bool _interrupted = false;
};
