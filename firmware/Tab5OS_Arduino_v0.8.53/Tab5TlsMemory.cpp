#include "Tab5TlsMemory.h"

#include <atomic>
#include <stdint.h>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

namespace {
std::atomic<bool> attempted{false};
std::atomic<bool> external{false};
std::atomic<uint32_t> failures{0};
std::atomic<size_t> lastFailedBytes{0};

#if defined(MBEDTLS_PLATFORM_MEMORY) && \
    !defined(MBEDTLS_PLATFORM_CALLOC_MACRO) && \
    !defined(MBEDTLS_PLATFORM_FREE_MACRO)
void* externalCalloc(size_t count, size_t bytes) {
  if (count != 0 && bytes > SIZE_MAX / count) {
    lastFailedBytes.store(SIZE_MAX, std::memory_order_relaxed);
    failures.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
  }
  const size_t total = count * bytes;
  // This is the same capability allocation used by ESP-IDF's official
  // CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC implementation. Never silently consume
  // the DMA/task-stack SRAM reserve if an external allocation fails.
  void* result = heap_caps_calloc(count, bytes,
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!result && total != 0) {
    lastFailedBytes.store(total, std::memory_order_relaxed);
    failures.fetch_add(1, std::memory_order_relaxed);
  }
  return result;
}

void releaseAllocation(void* pointer) {
  // heap_caps_free also accepts allocations made by the previous SDK
  // allocator, should an object have been initialized earlier during boot.
  heap_caps_free(pointer);
}
#endif
}  // namespace

bool tab5TlsMemoryBegin() {
  if (attempted.exchange(true, std::memory_order_acq_rel)) {
    return external.load(std::memory_order_acquire);
  }
#if defined(MBEDTLS_PLATFORM_MEMORY) && \
    !defined(MBEDTLS_PLATFORM_CALLOC_MACRO) && \
    !defined(MBEDTLS_PLATFORM_FREE_MACRO)
  if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) == 0) {
    return false;
  }
  // Install once, never around an individual connection: printer, radio and
  // other TLS clients can run concurrently and share these function pointers.
  if (mbedtls_platform_set_calloc_free(externalCalloc, releaseAllocation) != 0) {
    return false;
  }
  external.store(true, std::memory_order_release);
#endif
  return external.load(std::memory_order_acquire);
}

bool tab5TlsMemoryExternal() {
  return external.load(std::memory_order_acquire);
}

Tab5TlsMemoryFailures tab5TlsMemoryFailures() {
  return {failures.load(std::memory_order_relaxed),
          lastFailedBytes.load(std::memory_order_relaxed)};
}
