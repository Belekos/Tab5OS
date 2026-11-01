#include "Tab5LvglMemory.h"
#include <lvgl.h>
#include <stdlib/builtin/lv_tlsf.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <new>

#if LV_USE_STDLIB_MALLOC != LV_STDLIB_CUSTOM || !LV_USE_TLSF
#error "Tab5OS requires its supplied LVGL configuration (custom pool adapter and native TLSF)"
#endif

namespace {
constexpr size_t SegmentBytes = 64U * 1024U;
struct Pool {
  Pool* next;
  lv_pool_t handle;
  size_t bytes;
  size_t live;
  bool owned;
};
StaticSemaphore_t mutexStorage;
SemaphoreHandle_t mutex = nullptr;
lv_tlsf_t allocator = nullptr;
Pool* pools = nullptr;
Tab5LvglMemoryStats stats{};
class Guard {
 public:
  Guard() : held(mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {}
  ~Guard() { if (held) xSemaphoreGive(mutex); }
  explicit operator bool() const { return held; }
 private:
  bool held;
};
size_t headerBytes() {
  const size_t alignment = lv_tlsf_align_size();
  return (sizeof(Pool) + alignment - 1) & ~(alignment - 1);
}
Pool* containing(const void* pointer) {
  const auto address = reinterpret_cast<uintptr_t>(pointer);
  for (Pool* pool = pools; pool; pool = pool->next) {
    const auto begin = reinterpret_cast<uintptr_t>(pool);
    if (address >= begin + headerBytes() && address < begin + pool->bytes) return pool;
  }
  return nullptr;
}
Pool* add(void* memory, size_t bytes, bool owned) {
  const size_t overhead = headerBytes() + lv_tlsf_pool_overhead();
  if (!memory || bytes <= overhead || bytes - overhead < lv_tlsf_block_size_min() ||
      bytes - overhead >= lv_tlsf_block_size_max() ||
      reinterpret_cast<uintptr_t>(memory) % lv_tlsf_align_size()) return nullptr;
  auto* pool = new (memory) Pool{};
  pool->handle = lv_tlsf_add_pool(allocator,
      static_cast<uint8_t*>(memory) + headerBytes(), bytes - headerBytes());
  if (!pool->handle) return nullptr;
  pool->bytes = bytes; pool->owned = owned; pool->next = pools; pools = pool;
  ++stats.pools;
  if (owned) { stats.ownedBytes += bytes; ++stats.backingAllocations; }
  return pool;
}
void remove(Pool* pool) {
  // TLSF may only remove a coalesced, empty pool. Live LVGL pointers never move.
  if (!pool || pool->live) return;
  Pool** link = &pools;
  while (*link && *link != pool) link = &(*link)->next;
  if (!*link) return;
  *link = pool->next;
  lv_tlsf_remove_pool(allocator, pool->handle);
  --stats.pools;
  if (pool->owned) {
    stats.ownedBytes -= pool->bytes; ++stats.backingReleases;
    heap_caps_free(pool);
  }
}
Pool* grow(size_t requested) {
  // TLSF rounds searches to a size class. Spare 1/32 plus alignment prevents
  // an otherwise large-enough exact backing block from falling below it.
  const size_t overhead = headerBytes() + lv_tlsf_pool_overhead() +
      lv_tlsf_block_size_min() + 2 * lv_tlsf_align_size();
  if (!requested || requested >= lv_tlsf_block_size_max() ||
      requested > SIZE_MAX - requested / 32 - overhead) return nullptr;
  const size_t required = requested + requested / 32 + overhead;
  const size_t preferred = std::max(SegmentBytes, required);
  void* memory = heap_caps_malloc(preferred, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  size_t bytes = preferred;
  if (!memory && preferred != required) {
    bytes = required;
    memory = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (!memory) return nullptr;
  Pool* pool = add(memory, bytes, true);
  if (!pool) heap_caps_free(memory);
  return pool;
}
void allocated(void* pointer) {
  ++containing(pointer)->live; ++stats.liveBlocks;
  stats.usedBytes += lv_tlsf_block_size(pointer);
  stats.peakUsedBytes = std::max(stats.peakUsedBytes, stats.usedBytes);
}
void* allocate(size_t bytes) {
  if (!allocator || !bytes) return nullptr;
  void* result = lv_tlsf_malloc(allocator, bytes);
  if (result) { allocated(result); return result; }
  Pool* grown = grow(bytes);
  if (!grown) return nullptr;
  result = lv_tlsf_malloc(allocator, bytes);
  if (result) allocated(result);
  if (!grown->live) remove(grown);
  return result;
}
void walker(void*, size_t bytes, int used, void* context) {
  auto& result = *static_cast<lv_mem_monitor_t*>(context);
  result.total_size += bytes;
  if (used) ++result.used_cnt;
  else {
    ++result.free_cnt; result.free_size += bytes;
    result.free_biggest_size = std::max(result.free_biggest_size, bytes);
  }
}
}
extern "C" {
void lv_mem_init(void) {
  if (!mutex) mutex = xSemaphoreCreateMutexStatic(&mutexStorage);
  Guard guard;
  if (!guard || allocator) return;
  void* control = heap_caps_malloc(lv_tlsf_size(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!control) return;
  allocator = lv_tlsf_create(control);
  if (!allocator) { heap_caps_free(control); return; }
  stats = {}; stats.ownedBytes = lv_tlsf_size(); ++stats.backingAllocations;
}
void lv_mem_deinit(void) {
  Guard guard;
  if (!guard || !allocator) return;
  // lv_deinit has already destroyed the UI. External pools remain caller-owned.
  while (pools) {
    Pool* next = pools->next;
    if (pools->owned) heap_caps_free(pools);
    pools = next;
  }
  lv_tlsf_destroy(allocator); heap_caps_free(allocator); allocator = nullptr;
  stats = {};
}
lv_mem_pool_t lv_mem_add_pool(void* memory, size_t bytes) {
  Guard guard;
  if (!guard || !allocator) return nullptr;
  Pool* pool = add(memory, bytes, false);
  return pool ? pool->handle : nullptr;
}
void lv_mem_remove_pool(lv_mem_pool_t handle) {
  Guard guard;
  if (!guard || !allocator) return;
  for (Pool* pool = pools; pool; pool = pool->next)
    if (pool->handle == handle) { remove(pool); return; }
}
void* lv_malloc_core(size_t bytes) { Guard guard; return guard ? allocate(bytes) : nullptr; }
void* lv_realloc_core(void* data, size_t bytes) {
  Guard guard;
  if (!guard || !allocator) return nullptr;
  if (!data) return allocate(bytes);
  Pool* oldPool = containing(data);
  if (!oldPool) return nullptr;
  const size_t previous = lv_tlsf_block_size(data);
  if (!bytes) {
    lv_tlsf_free(allocator, data); --oldPool->live; --stats.liveBlocks;
    stats.usedBytes -= previous;
    if (oldPool->owned && !oldPool->live) remove(oldPool);
    return nullptr;
  }
  // Native TLSF can shrink/extend in place. Its moving copy runs under this
  // ordinary task mutex, outside ESP-IDF's IRQ-masking shared-heap lock.
  void* result = lv_tlsf_realloc(allocator, data, bytes);
  Pool* grown = nullptr;
  if (!result) {
    grown = grow(bytes);
    if (grown) result = lv_tlsf_realloc(allocator, data, bytes);
  }
  if (result) {
    Pool* nextPool = containing(result);
    if (nextPool != oldPool) { --oldPool->live; ++nextPool->live; }
    stats.usedBytes = stats.usedBytes - previous + lv_tlsf_block_size(result);
    stats.peakUsedBytes = std::max(stats.peakUsedBytes, stats.usedBytes);
    if (oldPool->owned && !oldPool->live) remove(oldPool);
  }
  if (grown && !grown->live) remove(grown);
  return result;
}
void lv_free_core(void* data) {
  if (!data) return;
  Guard guard;
  if (!guard || !allocator) return;
  Pool* pool = containing(data);
  if (!pool) return;
  stats.usedBytes -= lv_tlsf_block_size(data); --stats.liveBlocks; --pool->live;
  lv_tlsf_free(allocator, data);
  if (pool->owned && !pool->live) remove(pool);
}
void lv_mem_monitor_core(lv_mem_monitor_t* result) {
  if (!result) return;
  std::memset(result, 0, sizeof(*result));
  Guard guard;
  if (!guard || !allocator) return;
  for (Pool* pool = pools; pool; pool = pool->next)
    lv_tlsf_walk_pool(pool->handle, walker, result);
  if (result->total_size)
    result->used_pct = 100U - uint64_t(100) * result->free_size / result->total_size;
  if (result->free_size)
    result->frag_pct = 100U - uint64_t(100) * result->free_biggest_size / result->free_size;
  result->max_used = stats.peakUsedBytes;
}
lv_result_t lv_mem_test_core(void) {
  Guard guard;
  if (!guard || !allocator || lv_tlsf_check(allocator)) return LV_RESULT_INVALID;
  for (Pool* pool = pools; pool; pool = pool->next)
    if (lv_tlsf_check_pool(pool->handle)) return LV_RESULT_INVALID;
  return LV_RESULT_OK;
}
}
Tab5LvglMemoryStats tab5LvglMemoryStats() {
  Guard guard;
  return guard ? stats : Tab5LvglMemoryStats{};
}
