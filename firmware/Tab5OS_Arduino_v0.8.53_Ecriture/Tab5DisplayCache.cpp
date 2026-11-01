#include "Tab5DisplayCache.h"
#include "Tab5DisplayBandwidth.h"
#include "Tab5DisplayPresent.h"
#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "Tab5DisplayCacheRanges.h"
#include "Tab5DisplayCacheRecovery.h"
#include <M5Unified.h>
#include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>
#include <atomic>
#include <esp_cache.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
namespace Ranges = Tab5DisplayCacheRanges;
std::atomic<TaskHandle_t> owner{nullptr};
Ranges::Row* rows = nullptr;
Ranges::Geometry geometry{};
uint32_t configuredBase = 0;
bool active = false;
bool usable = false;
bool hasRect = false;
bool filtered = false;
bool fallback = false;
bool mergedWriteback = false;
uint32_t frameStartErrors = 0;
Tab5DisplayCacheRecovery::Retry recovery;
Tab5DisplayCacheStats stats;

uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

bool isOwner() {
  const TaskHandle_t task = owner.load(std::memory_order_acquire);
  return task && task == xTaskGetCurrentTaskHandle();
}

bool readGeometry(Ranges::Geometry& result) {
  // Only M5Unified's known Tab5 DSI panel has the linear RGB565 layout below.
  // In particular, do not cast arbitrary panels used by external displays.
  if (M5.getBoard() != m5::board_t::board_M5Tab5) return false;
  auto* panel = M5.Display.getPanel();
  if (!panel || !panel->getAutoDisplay() || panel->getStartCount() != 0
      || !panel->getBus()
      || panel->getBus()->busType() != lgfx::bus_type_t::bus_dsi) return false;
  const auto& cfg = panel->config();
  if (cfg.panel_width != 720 || cfg.panel_height != 1280
      || cfg.memory_width != cfg.panel_width || cfg.memory_height != cfg.panel_height
      || cfg.offset_x || cfg.offset_y || M5.Display.getColorDepth() != 16) return false;
  const auto* dsi = static_cast<lgfx::Panel_DSI*>(panel);
  void* buffer = dsi->config_detail().buffer;
  if (!buffer || !esp_ptr_external_ram(buffer)) return false;
  const uint32_t stride = (uint32_t(cfg.panel_width) * 2 + 3) & ~uint32_t(3);
  return Ranges::configure(result, uint32_t(reinterpret_cast<uintptr_t>(buffer)),
                           cfg.panel_width, cfg.panel_height, stride,
                           panel->getRotation(), cfg.offset_rotation);
}

void recordMirrorSpan(uint16_t y, uint16_t first, uint16_t end) {
  if (rows[y].first > first) rows[y].first = first;
  if (rows[y].end < end) rows[y].end = end;
}

int syncRange(uint32_t first, uint32_t end, bool count) {
  while (first < end) {
    const uint32_t bytes = (end - first < Ranges::kChunkBytes)
                         ? end - first : Ranges::kChunkBytes;
    const esp_err_t result = esp_cache_msync(
        reinterpret_cast<void*>(static_cast<uintptr_t>(first)), bytes,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
    if (count) {
      ++stats.cacheCalls;
      ++stats.lastCacheCalls;
      stats.syncedBytes += bytes;
      stats.lastSyncedBytes += bytes;
      if (result != ESP_OK) {
        ++stats.cacheErrors;
        stats.lastCacheError = result;
      }
    }
    if (result != ESP_OK) return result;
    first += bytes;
  }
  return ESP_OK;
}
}  // namespace

bool tab5DisplayCacheInit() {
  TaskHandle_t expected = nullptr;
  const TaskHandle_t current = xTaskGetCurrentTaskHandle();
  if (!owner.compare_exchange_strong(expected, current, std::memory_order_acq_rel)
      && expected != current) return false;
  Ranges::Geometry candidate{};
  if (active || !readGeometry(candidate)) return false;
  tab5DisplayBandwidthInit();
  if (!rows) rows = static_cast<Ranges::Row*>(heap_caps_malloc(
      sizeof(Ranges::Row) * candidate.height, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!rows) return false;
  geometry = candidate;
  configuredBase = candidate.base;
  stats.configured = true;
  return true;
}

bool tab5DisplayCacheBeginFrame() {
  if (!isOwner()) return false;
  if (active) { usable = false; return false; }
  Ranges::Geometry before{};
  mergedWriteback = rows && readGeometry(before)
      && tab5DisplayPresentOwnsBuffer(before.base)
      && before.width == geometry.width && before.height == geometry.height
      && before.stride == geometry.stride;
  if (mergedWriteback) Ranges::clear(geometry, rows);
  if (!tab5DisplayPresentBeginFrame(mergedWriteback ? recordMirrorSpan : nullptr)) {
    mergedWriteback = false;
    return false;
  }
  active = true;
  hasRect = filtered = fallback = false;
  stats.lastRequestedBytes = stats.lastSyncedBytes = stats.lastCacheCalls = 0;
  frameStartErrors = stats.cacheErrors;
  Ranges::Geometry current{};
  usable = rows && readGeometry(current)
        && (current.base == configuredBase || tab5DisplayPresentOwnsBuffer(current.base))
        && current.width == geometry.width && current.height == geometry.height
        && current.stride == geometry.stride;
  if (mergedWriteback && !usable) {
    // The mirrored pixels have not been written back: never publish a frame
    // whose geometry cannot be verified. The presenter retries a full mirror.
    tab5DisplayPresentEndFrame(false);
    active = mergedWriteback = false;
    return false;
  }
  if (usable) {
    geometry = current;  // Rotation may have changed since the previous frame.
    if (recovery.beginAttempt(nowMs())) {
      ++stats.recoveryAttempts;
      uint32_t first, end;
      if (Ranges::alignedRange(geometry.base, geometry.end - geometry.base, first, end)) {
        stats.requestedBytes += end - first;
        stats.lastRequestedBytes += end - first;
        fallback = true;
        if (syncRange(first, end, true) == ESP_OK) {
          recovery.recovered();
          ++stats.recoveries;
        }
      }
    }
    if (!mergedWriteback) Ranges::clear(geometry, rows);
  }
  return true;
}

void tab5DisplayCacheDirtyRect(int32_t x, int32_t y, int32_t width, int32_t height) {
  if (!isOwner() || !active) return;
  tab5DisplayPresentDirtyRect(x, y, width, height);
  if (!usable) return;
  if (!Ranges::addRect(geometry, rows, x, y, width, height)) usable = false;
  else hasRect = true;
}

void tab5DisplayCacheEndFrame() {
  if (!isOwner() || !active) return;
  const bool prepared = tab5DisplayPresentPrepareFrame(usable);
  if (mergedWriteback && prepared) {
    uint32_t first, end;
    int result = ESP_ERR_INVALID_ARG;
    if (Ranges::alignedRange(geometry.base, geometry.end - geometry.base, first, end)) {
      if (usable) {
        result = ESP_OK;
        const bool synced = Ranges::emitRanges(geometry, rows, first, end,
            [&](uint32_t a, uint32_t b) {
              result = syncRange(a, b, true);
              return result == ESP_OK;
            });
        if (!synced && result == ESP_OK) result = ESP_ERR_INVALID_ARG;
      } else {
        // An untracked write has used the generic hook during this frame.
        // Include the mirror as well before handing the buffer to DMA.
        fallback = true;
        result = syncRange(first, end, true);
      }
    }
    if (result != ESP_OK) recovery.failed(nowMs());
    mergedWriteback = false;
  }
  mergedWriteback = false;
  active = false;
  ++stats.frames;
  if (filtered) ++stats.filteredFrames;
  if (fallback) ++stats.fallbackFrames;
  tab5DisplayPresentEndFrame(prepared && stats.cacheErrors == frameStartErrors && !recovery.pending());
}

Tab5DisplayCacheStats tab5DisplayCacheStats() {
  if (!isOwner()) return {};
  auto result = stats;
  result.pendingRecovery = recovery.pending();
  return result;
}

bool tab5DisplayCacheNeedsRedraw() {
  if (!isOwner()) return false;
  const bool presentRedraw = tab5DisplayPresentNeedsRedraw();
  if (!recovery.requestRedraw(nowMs())) return presentRedraw;
  ++stats.recoveryRequests;
  return true;
}

// M5GFX 0.2.30 supplies this C++ function as a weak extension point. Preserve
// its C2M DATA semantics and 128-byte alignment. Never invalidate dirty pixels.
// The existing 16 KiB chunk limit is also retained for every fallback path.
// Reduced ranges mean fewer cache addresses examined, not measured PSRAM bytes.
int Cache_WriteBack_Addr(uint32_t addr, uint32_t size) {
  uint32_t first, end;
  if (!Ranges::alignedRange(addr, size, first, end)) return ESP_ERR_INVALID_ARG;
  if (first >= end) return ESP_OK;
  // Check ownership BEFORE reading any mutable UI-task tracking state. Calls
  // from other tasks (or outside a tracked LVGL transaction) always sync fully.
  const bool track = isOwner() && active;
  if (track) {
    stats.requestedBytes += end - first;
    stats.lastRequestedBytes += end - first;
  }
  const bool knownRange = track && usable && hasRect && addr >= geometry.base
                      && uint64_t(addr) + size <= geometry.end;
  if (!knownRange) {
    if (track) { fallback = true; usable = false; }
    const int result = syncRange(first, end, track);
    if (track && result != ESP_OK && rows) recovery.failed(nowMs());
    return result;
  }
  if (mergedWriteback) {
    // The back buffer is not scanned. EndFrame synchronizes the union of the
    // previous mirror and all current LVGL areas, before publication.
    filtered = true;
    return ESP_OK;
  }
  int result = ESP_OK;
  bool emitted = false;
  const bool success = Ranges::emitRanges(geometry, rows, first, end,
      [&](uint32_t a, uint32_t b) {
        emitted = true;
        result = syncRange(a, b, true);
        return result == ESP_OK;
      });
  if (!success || !emitted) {
    // M5GFX ignores this hook's return code and resets its dirty bounds. Retry
    // the whole requested span immediately; retain a full-buffer retry if the
    // cache API still fails, instead of losing those unsynchronized pixels.
    fallback = true;
    usable = false;
    result = syncRange(first, end, true);
    if (result != ESP_OK) recovery.failed(nowMs());
    return result;
  }
  filtered = true;
  return result;
}

#else
bool tab5DisplayCacheInit() { return false; }
bool tab5DisplayCacheBeginFrame() { return true; }
void tab5DisplayCacheDirtyRect(int32_t, int32_t, int32_t, int32_t) {}
void tab5DisplayCacheEndFrame() {}
bool tab5DisplayCacheNeedsRedraw() { return false; }
Tab5DisplayCacheStats tab5DisplayCacheStats() { return {}; }
#endif
