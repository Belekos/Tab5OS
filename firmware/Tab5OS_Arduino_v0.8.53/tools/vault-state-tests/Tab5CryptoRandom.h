#pragma once
#include <cstddef>
#include <cstdint>
namespace Tab5CryptoRandom {inline bool fill(void* out,size_t n){static unsigned count=0;auto* p=static_cast<uint8_t*>(out);while(n--)*p++=++count;return true;}}
