#pragma once
#include <stddef.h>

// Heap capacities, not physical chip capacities. INTERNAL includes the HP,
// SPM/TCM and LP heaps on ESP32-P4: it must not be added to those sub-pools.
namespace Tab5MemoryLayout {
struct Pool {
  size_t total=0, free=0, minimumFree=0, largestFree=0;
  size_t used() const { return total>free ? total-free : 0; }
};
struct Snapshot {
  Pool internal, hp, tcm, lp, psram;
  size_t hpPhysicalBytes=0, tcmPhysicalBytes=0, lpPhysicalBytes=0;
  size_t l2CacheBytes=0;
  bool splitAvailable=false, largestSampled=false;
};
// Counter-only by default. Largest-free-block queries walk allocator blocks
// with heap locks held; request them only for explicit diagnostics, not frames.
// Samples across pools are sequential, not an atomic memory snapshot. Minimum
// free is the sum of each region's low-water mark, not a global instant minimum.
Snapshot sample(bool includeLargest=false);
}
