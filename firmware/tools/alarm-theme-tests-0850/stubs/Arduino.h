#pragma once
#include <cstdint>
#include <string>
extern uint32_t testMillis;
inline uint32_t millis(){return testMillis;}
inline void delay(unsigned n){testMillis+=n;}
class String: public std::string {public:using std::string::string;};
