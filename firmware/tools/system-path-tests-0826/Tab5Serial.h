#pragma once
// Serial sink only; storage logic remains the production implementation.
struct Tab5TestDebugSerial {template<class... T> void printf(const char*,T...){} };
inline Tab5TestDebugSerial Tab5DebugSerial;
