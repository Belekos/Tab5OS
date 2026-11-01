#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include <esp_attr.h>
#include <esp_cache.h>
#include <esp_idf_version.h>
#include <esp_memory_utils.h>
#include <hal/cache_hal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <stdint.h>

extern "C" esp_err_t __real_esp_cache_msync(void*, size_t, int);

#if ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 4)
namespace {
constexpr size_t ChunkBytes = 16 * 1024;
constexpr int KnownFlags = ESP_CACHE_MSYNC_FLAG_INVALIDATE | ESP_CACHE_MSYNC_FLAG_UNALIGNED
    | ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_DIR_M2C
    | ESP_CACHE_MSYNC_FLAG_TYPE_DATA | ESP_CACHE_MSYNC_FLAG_TYPE_INST;

esp_err_t IRAM_ATTR syncChunks(uintptr_t first, size_t size, uint32_t line, int flags) {
  while (size) {
    // An unaligned first byte must not make a chunk touch a 129th cache line.
    const size_t room = ChunkBytes - (first & (line - 1));
    const size_t count = size < room ? size : room;
    const esp_err_t result = __real_esp_cache_msync(reinterpret_cast<void*>(first), count, flags);
    if (result != ESP_OK) return result;
    first += count;
    size -= count;
  }
  return ESP_OK;
}
}
#endif

// The native API remains responsible for each cache operation and its lock.
// Returning between chunks lets a pending DSI interrupt rearm scanout. This
// does not make shared buffers safe to modify before their owner completes.
extern "C" esp_err_t IRAM_ATTR __wrap_esp_cache_msync(void* address, size_t size, int flags) {
#if ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 4)
  const uintptr_t first = reinterpret_cast<uintptr_t>(address);
  const bool m2c = (flags & ESP_CACHE_MSYNC_FLAG_DIR_M2C) != 0;
  const bool unaligned = (flags & ESP_CACHE_MSYNC_FLAG_UNALIGNED) != 0;
  const bool invalidate = (flags & ESP_CACHE_MSYNC_FLAG_INVALIDATE) != 0;
  // ISR and already-critical calls retain the native cache-safe path. Do not
  // change instruction-cache operations, unknown flags or invalid arguments.
  if (size > ChunkBytes && first && first <= UINT32_MAX
      && size <= UINT32_MAX - first && !(flags & ~KnownFlags)
      && !(flags & ESP_CACHE_MSYNC_FLAG_TYPE_INST)
      && !(m2c && (flags & ESP_CACHE_MSYNC_FLAG_DIR_C2M))
      && !(m2c && unaligned) && !xPortInIsrContext() && xPortCanYield()
      && esp_ptr_external_ram(address)
      && esp_ptr_external_ram(reinterpret_cast<void*>(first + size - 1))) {
    // These are the same pure range/alignment checks as IDF 5.5.4, performed
    // on the WHOLE request before any cache operation. Never preflight by
    // synchronizing the whole range, or partially act on an invalid request.
    uint32_t level = 0, id = 0;
    if (cache_hal_vaddr_to_cache_level_id(uint32_t(first), uint32_t(size), &level, &id)) {
      const uint32_t line = cache_hal_get_cache_line_size(level, CACHE_TYPE_DATA);
      if (line && line <= ChunkBytes && !(line & (line - 1)) && !(ChunkBytes % line)) {
        const bool aligned = !(first & (line - 1)) && !(size & (line - 1));
        // Preserve the native unaligned writeback+invalidate operation as a
        // unit: converting its invalidation to M2C would change its accepted
        // byte range. JPEG data/output and aligned ELF segments are covered.
        if ((unaligned || aligned) && !(!m2c && invalidate && !aligned)) {
          if (!m2c && invalidate) {
            // Native C2M|INVALIDATE writes back the entire range first. Keep
            // that ordering instead of interleaving invalidations per chunk.
            const esp_err_t result = syncChunks(first, size, line,
                flags & ~ESP_CACHE_MSYNC_FLAG_INVALIDATE);
            if (result != ESP_OK) return result;
            const int invalidationFlags = (flags & ~(ESP_CACHE_MSYNC_FLAG_DIR_C2M
                | ESP_CACHE_MSYNC_FLAG_UNALIGNED)) | ESP_CACHE_MSYNC_FLAG_DIR_M2C;
            return syncChunks(first, size, line, invalidationFlags);
          }
          return syncChunks(first, size, line, flags);
        }
      }
    }
  }
#endif
  return __real_esp_cache_msync(address, size, flags);
}
#endif
