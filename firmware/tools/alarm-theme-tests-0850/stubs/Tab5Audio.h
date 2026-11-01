#pragma once
#include <cstdint>
extern unsigned leaseAcquired,leaseReleased;
struct AudioStub {uint32_t acquireMemoryPriority(unsigned){++leaseAcquired;return 7;}void releaseMemoryPriority(uint32_t){++leaseReleased;}};
extern AudioStub Tab5Audio;
