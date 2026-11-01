#pragma once
#include <stdint.h>

namespace Tab5DisplayBandwidthPolicy {
constexpr uint32_t kMemoryReadShift = 16;
constexpr uint32_t kMemoryReadMask = 15u << kMemoryReadShift;

constexpr uint8_t displayRead(uint32_t priorities) {
  return (priorities >> kMemoryReadShift) & 15u;
}

constexpr uint8_t targetRead(uint32_t priorities) {
  uint8_t highestOther = 0;
  for (uint32_t shift = 0; shift < 32; shift += 4) {
    if (shift == kMemoryReadShift) continue;
    const uint8_t value = (priorities >> shift) & 15u;
    if (value > highestOther) highestOther = value;
  }
  const uint8_t target = highestOther < 15 ? highestOther + 1 : 15;
  const uint8_t initial = displayRead(priorities);
  return initial > target ? initial : target;
}

constexpr bool verify(uint32_t originalRead, uint32_t originalWrite,
                      uint32_t currentRead, uint32_t currentWrite,
                      uint8_t target) {
  return currentWrite == originalWrite
      && ((currentRead ^ originalRead) & ~kMemoryReadMask) == 0
      && displayRead(currentRead) == target;
}
}

