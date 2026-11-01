#pragma once
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstring>
using String=std::string;
inline uint32_t millis(){static uint32_t t=0;return ++t;}
