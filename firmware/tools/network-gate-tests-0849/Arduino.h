#pragma once
#include <cstdint>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>
inline uint32_t millis(){static const auto start=std::chrono::steady_clock::now();return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();}
extern std::atomic<unsigned> delayCalls;
inline void delay(unsigned ms){++delayCalls;std::this_thread::sleep_for(std::chrono::milliseconds(ms));}
struct TestSerial{template<class...T>void printf(const char*,T...) {}};
extern TestSerial Tab5DebugSerial;
