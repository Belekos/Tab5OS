#pragma once
#include <cstddef>
#include <cstdint>
inline uint32_t millis() { static uint32_t now; return ++now; }
struct MockDebugSerial { void printf(const char*, ...) {} };
inline MockDebugSerial Tab5DebugSerial;
