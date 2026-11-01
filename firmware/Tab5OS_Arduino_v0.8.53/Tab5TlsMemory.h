#pragma once

#include <stddef.h>
#include <stdint.h>

// Call once during boot, after PSRAM initialization and before starting any
// TLS user or concurrent network worker. The mbedTLS allocator is global.
// Returns false if PSRAM or the public runtime allocator API is unavailable;
// in that case the SDK's original allocator is retained.
bool tab5TlsMemoryBegin();
bool tab5TlsMemoryExternal();

struct Tab5TlsMemoryFailures {
  uint32_t count;
  size_t lastBytes;
};

// Allocation callbacks never log, acquire application locks or invoke cache
// reclamation. A consumer can inspect these counters from normal task context.
Tab5TlsMemoryFailures tab5TlsMemoryFailures();
