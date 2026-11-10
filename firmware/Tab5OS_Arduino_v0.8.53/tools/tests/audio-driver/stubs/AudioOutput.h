#pragma once
#include <cstdint>
class AudioOutput {
 public:
  virtual ~AudioOutput() = default;
  virtual bool begin() { return true; }
  virtual bool ConsumeSample(int16_t[2]) { return true; }
  virtual bool SetRate(int) { return true; }
  virtual bool SetChannels(int) { return true; }
  virtual void flush() {}
  virtual bool stop() { return true; }
 protected:
  int hertz = 0;
  uint8_t channels = 0;
  uint32_t gainF2P6 = 0;
};
