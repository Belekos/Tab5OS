#pragma once
#include <stddef.h>
struct Tab5LvglMemoryStats {
  size_t ownedBytes, pools, liveBlocks, usedBytes, peakUsedBytes;
  size_t backingAllocations, backingReleases;
};
Tab5LvglMemoryStats tab5LvglMemoryStats();
