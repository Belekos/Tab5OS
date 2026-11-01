#pragma once
#include <stddef.h>
#include <stdint.h>

// Pure policies, also tested on the PC. These are ceilings, not reservations.
namespace Tab5Resources {
constexpr size_t KiB = 1024U;
constexpr size_t MiB = 1024U * KiB;

struct MemorySample {
  size_t internal = 0, internalBlock = 0, dmaBlock = 0, psram = 0, psramBlock = 0;
  // Measured keeps the public largest-block diagnostic/legacy sample contract.
  // Fit/NoFit are a bounded query for the requested size, not a fake maximum.
  enum class Block : uint8_t { Measured, Fit, NoFit, Unknown };
  Block internalFit = Block::Measured, dmaFit = Block::Measured, psramFit = Block::Measured;
};
struct MemoryTotals {
  size_t internal = 0, psram = 0;
};
inline size_t radioCapacity(size_t freeBytes, size_t largestBlock) {
  // Spend at most 1/16 of currently free PSRAM. Leave room for the UI, maps
  // and future applications. No allocation happens before a stream opens.
  size_t target = freeBytes / 16U;
  // The user's France Culture probe received ~600 kB in its initial burst.
  // 512 KiB filled before that burst ended, leaving traffic in the TCP heap.
  if (target > MiB) target = MiB;
  if (target > largestBlock / 2U) target = largestBlock / 2U;
  target -= target % (16U * KiB);
  return target >= 64U * KiB ? target : 0;
}

inline size_t radioPreload(size_t capacity) {
  const size_t quarter = capacity / 4U;
  return quarter < 96U * KiB ? quarter : 96U * KiB;
}

inline size_t growthCapacity(size_t current, size_t needed, size_t limit) {
  if (needed > limit) return 0;
  if (current >= needed) return current;
  size_t target = current ? current : (16U * KiB);
  if (target > limit) target = limit;
  while (target < needed) {
    if (target > limit / 2U) return limit;
    target *= 2U;
  }
  return target;
}

inline bool retryRadioInsteadOfPause(bool radio, bool paused,
                                    bool decoderRunning) {
  return radio && !paused && !decoderRunning;
}
}  // namespace Tab5Resources
