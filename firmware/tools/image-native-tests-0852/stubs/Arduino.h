#pragma once
#include <cstdint>
extern uint32_t testMillis;
inline uint32_t millis(){return testMillis;}
inline void delay(unsigned n){testMillis+=n;}
