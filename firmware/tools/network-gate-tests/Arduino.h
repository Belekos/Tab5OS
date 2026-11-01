#pragma once
#include <stdint.h>
extern uint32_t testMilliseconds;
extern void (*testDelayHook)();
inline uint32_t millis() { return testMilliseconds; }
inline void delay(unsigned value) {
  testMilliseconds += value;
  if (testDelayHook) testDelayHook();
}
struct TestSerial { template<class... T> void printf(const char*, T...) {} };
extern TestSerial Serial;
