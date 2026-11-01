#pragma once

#include <FS.h>
#include <cstring>

// ArduinoJson's default Stream reader requests one byte through readBytes().
// On a FAT SD card even filtered metadata therefore incurs a filesystem call
// per byte. Buffer the stream without allocating a second copy of the JSON.
// Used only during synchronous UI loading; 1 KiB is bounded on the UI stack.
class Tab5JsonFileReader {
 public:
  explicit Tab5JsonFileReader(File& file) : _file(file) {}
  int read() {
    if (_next == _end && !refill()) return -1;
    return _buffer[_next++];
  }
  size_t readBytes(char* destination, size_t bytes) {
    size_t copied = 0;
    while (copied < bytes) {
      if (_next == _end && !refill()) break;
      const size_t available = _end - _next;
      const size_t count = bytes - copied < available ? bytes - copied : available;
      memcpy(destination + copied, _buffer + _next, count);
      _next += count;
      copied += count;
    }
    return copied;
  }

 private:
  bool refill() {
    _next = 0;
    _end = _file.read(_buffer, sizeof(_buffer));
    // ESP32 File::read(buffer, size) returns size_t(-1) for an invalid File.
    if (_end > sizeof(_buffer)) _end = 0;
    return _end != 0;
  }
  File& _file;
  uint8_t _buffer[1024];
  size_t _next = 0, _end = 0;
};
