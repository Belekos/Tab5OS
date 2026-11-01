#include "Tab5DisplayPresent.h"
#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "Tab5DisplayCacheRanges.h"
#include "Tab5DisplayPendingMirror.h"
#include "Tab5DisplayBlit.h"
#include "Tab5DisplayPresentFence.h"
#include <M5Unified.h>
#include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <esp_attr.h>
#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_lcd_panel_ops.h>
#include <esp_memory_utils.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
namespace Ranges = Tab5DisplayCacheRanges;
constexpr uint32_t kWaitMs = 100;
constexpr uint32_t kRetryMs = 1000;

// These are real base-class member pointers, not casts to a fictitious derived
// instance or offsets into a private SDK structure. This adapter is checked
// against the pinned M5GFX 0.2.30 / IDF 5.5.4 layout before it changes anything.
struct PanelAccess : lgfx::Panel_DSI {
  static auto handleMember() { return &PanelAccess::_disp_panel_handle; }
  static auto linesMember() { return &PanelAccess::_lines_buffer; }
};

std::atomic<TaskHandle_t> owner{nullptr};
std::atomic<uint32_t> refreshes{0};
std::atomic<uint32_t> lastRefreshUs{0}, maxRefreshGapUs{0};
static_assert(std::atomic<uint32_t>::is_always_lock_free, "ISR counter must be lock-free");
lgfx::Panel_DSI* panel = nullptr;
esp_lcd_panel_handle_t handle = nullptr;
uint8_t* buffers[2] = {};
uint8_t** lines = nullptr;
Ranges::Row* dirty = nullptr;
Tab5DisplayPendingMirror::Row* pendingMirror = nullptr;
Tab5DisplayMirrorSpan mirrorRecorder = nullptr;
bool deferredMirror = false;
Ranges::Geometry geometry{};
Tab5DisplayPresentStats stats;
uint32_t submittedAfter = 0;
uint32_t submittedAt = 0;
uint32_t retryAfter = 0;
bool drawing = false;
bool mirrorNeeded = true;
bool fullMirror = true;
bool repaintNeeded = false;
bool timeoutReported = false;
bool retryBlocked = false;
bool nativeTransferEnabled = false;

uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
bool isOwner() {
  const auto task = owner.load(std::memory_order_acquire);
  return task && task == xTaskGetCurrentTaskHandle();
}

bool IRAM_ATTR onRefresh(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t*, void*) {
  const uint32_t now = uint32_t(esp_timer_get_time());
  const uint32_t previous = lastRefreshUs.exchange(now, std::memory_order_relaxed);
  const uint32_t gap = now - previous;
  if (previous && gap > maxRefreshGapUs.load(std::memory_order_relaxed))
    maxRefreshGapUs.store(gap, std::memory_order_relaxed);
  refreshes.fetch_add(1, std::memory_order_release);
  return false;
}

bool fenceReleased() {
  return Tab5DisplayPresentFence::released(
      refreshes.load(std::memory_order_acquire), submittedAfter);
}

bool finishPending() {
  if (!stats.pending) return true;
  const uint32_t elapsed = nowMs() - submittedAt;
  if (elapsed > stats.maxWaitMs) stats.maxWaitMs = elapsed;
  if (!fenceReleased()) {
    if (elapsed >= kWaitMs && !timeoutReported) {
      ++stats.timeouts;
      stats.waiting = timeoutReported = true;
      stats.lastError = ESP_ERR_TIMEOUT;
    }
    return false;
  }
  stats.front = stats.back;
  stats.back ^= 1;
  stats.pending = stats.waiting = timeoutReported = false;
  ++stats.flips;
  mirrorNeeded = true;
  return true;
}

bool syncBytes(uint8_t* first, uint32_t count) {
  uint32_t begin, end;
  if (!Ranges::alignedRange(uint32_t(reinterpret_cast<uintptr_t>(first)), count, begin, end))
    return false;
  while (begin < end) {
    const uint32_t length = end - begin < Ranges::kChunkBytes ? end - begin : Ranges::kChunkBytes;
    const auto result = esp_cache_msync(reinterpret_cast<void*>(uintptr_t(begin)), length,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
    if (result != ESP_OK) { stats.lastError = result; return false; }
    begin += length;
  }
  return true;
}

bool mirrorFront(Tab5DisplayMirrorSpan mirrored) {
  uint8_t* source = buffers[stats.front];
  uint8_t* target = buffers[stats.back];
  // The front is read-only. If the cache owner records these spans, it writes
  // their union with the new drawing once, after rendering has finished.
  for (uint32_t y = 0; y < geometry.height; ++y) {
    const uint32_t first = fullMirror ? 0 : dirty[y].first;
    const uint32_t end = fullMirror ? geometry.width : dirty[y].end;
    if (first >= end) continue;
    const uint32_t offset = y * geometry.stride + first * 2;
    const uint32_t count = (end - first) * 2;
    memcpy(target + offset, source + offset, count);
    stats.mirroredBytes += count;
    if (mirrored) mirrored(uint16_t(y), uint16_t(first), uint16_t(end));
  }
  bool ok;
  if (mirrored) ok = true;
  else if (fullMirror) ok = syncBytes(target, geometry.stride * geometry.height);
  else {
    auto targetGeometry = geometry;
    targetGeometry.base = uint32_t(reinterpret_cast<uintptr_t>(target));
    targetGeometry.end = targetGeometry.base + geometry.stride * geometry.height;
    uint32_t first, end;
    ok = Ranges::alignedRange(targetGeometry.base, targetGeometry.end - targetGeometry.base, first, end)
      && Ranges::emitRanges(targetGeometry, dirty, first, end,
          [](uint32_t a, uint32_t b) { return syncBytes(reinterpret_cast<uint8_t*>(uintptr_t(a)), b - a); });
  }
  if (!ok) {
    ++stats.copyErrors;
    retryAfter = nowMs();
    retryBlocked = true;
    fullMirror = true;
    return false;
  }
  mirrorNeeded = fullMirror = retryBlocked = false;
  return true;
}

void copyPendingSpan(uint16_t y, uint16_t first, uint16_t end) {
  const uint32_t offset = uint32_t(y) * geometry.stride + uint32_t(first) * 2;
  const uint32_t count = uint32_t(end - first) * 2;
  memcpy(buffers[stats.back] + offset, buffers[stats.front] + offset, count);
  stats.mirroredBytes += count;
  mirrorRecorder(y, first, end);
}

bool selectBack() {
  if (panel != M5.Display.getPanel() || panel->getStartCount() != 0
      || M5.Display.getColorDepth() != 16) return false;
  const auto& cfg = panel->config();
  auto detail = panel->config_detail();
  if (cfg.panel_width != 720 || cfg.panel_height != 1280
      || cfg.memory_width != 720 || cfg.memory_height != 1280
      || cfg.offset_x || cfg.offset_y || !panel->getAutoDisplay()) return false;
  if (!Ranges::configure(geometry, uint32_t(reinterpret_cast<uintptr_t>(buffers[stats.back])),
      cfg.panel_width, cfg.panel_height, 1440, panel->getRotation(), cfg.offset_rotation)) return false;
  for (uint32_t y = 0; y < geometry.height; ++y) lines[y] = buffers[stats.back] + y * geometry.stride;
  detail.buffer = buffers[stats.back];
  panel->config_detail(detail);
  return true;
}
}

bool tab5DisplayPresentInit() {
  TaskHandle_t expected = nullptr;
  const auto current = xTaskGetCurrentTaskHandle();
  if (!owner.compare_exchange_strong(expected, current, std::memory_order_acq_rel) && expected != current)
    return false;
  if (stats.active) return true;
#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 4)
  stats.lastError = ESP_ERR_NOT_SUPPORTED;
  return false;
#else
  if (M5.getBoard() != m5::board_t::board_M5Tab5) return false;
  auto* candidate = M5.Display.getPanel();
  if (!candidate || !candidate->getBus() || candidate->getBus()->busType() != lgfx::bus_type_t::bus_dsi
      || candidate->getStartCount() || !candidate->getAutoDisplay() || M5.Display.getColorDepth() != 16)
    return false;
  const auto& cfg = candidate->config();
  if (cfg.panel_width != 720 || cfg.panel_height != 1280 || cfg.memory_width != 720
      || cfg.memory_height != 1280 || cfg.offset_x || cfg.offset_y) return false;
  panel = static_cast<lgfx::Panel_DSI*>(candidate);
  handle = panel->*PanelAccess::handleMember();
  lines = panel->*PanelAccess::linesMember();
  if (!handle || !lines) { panel = nullptr; return false; }
  void* firstBuffer = nullptr;
  void* secondBuffer = nullptr;
  const auto result = esp_lcd_dpi_panel_get_frame_buffer(handle, 2, &firstBuffer, &secondBuffer);
  buffers[0] = static_cast<uint8_t*>(firstBuffer);
  buffers[1] = static_cast<uint8_t*>(secondBuffer);
  if (result != ESP_OK || !buffers[0] || !buffers[1] || buffers[0] == buffers[1]
      || !esp_ptr_external_ram(buffers[0]) || !esp_ptr_external_ram(buffers[1])
      || (reinterpret_cast<uintptr_t>(buffers[0]) & 127) || (reinterpret_cast<uintptr_t>(buffers[1]) & 127)
      || panel->config_detail().buffer != buffers[0]) {
    stats.lastError = result != ESP_OK ? result : ESP_ERR_INVALID_STATE;
    return false;
  }
  for (uint32_t y = 0; y < 1280; ++y)
    if (lines[y] != buffers[0] + y * 1440) { stats.lastError = ESP_ERR_INVALID_STATE; return false; }
  if (!Ranges::configure(geometry, uint32_t(reinterpret_cast<uintptr_t>(buffers[0])),
      720, 1280, 1440, panel->getRotation(), cfg.offset_rotation)) return false;
  dirty = static_cast<Ranges::Row*>(heap_caps_malloc(sizeof(Ranges::Row) * 1280,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!dirty) { stats.lastError = ESP_ERR_NO_MEM; return false; }
  Ranges::clear(geometry, dirty);
  // Optional metadata only; a failed allocation retains the eager mirror path.
  pendingMirror = static_cast<Tab5DisplayPendingMirror::Row*>(heap_caps_malloc(
      sizeof(Tab5DisplayPendingMirror::Row) * geometry.height, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  esp_lcd_dpi_panel_event_callbacks_t callbacks{};
  callbacks.on_refresh_done = onRefresh;
  const auto registered = esp_lcd_dpi_panel_register_event_callbacks(handle, &callbacks, nullptr);
  if (registered != ESP_OK) {
    heap_caps_free(dirty); dirty = nullptr;
    heap_caps_free(pendingMirror); pendingMirror = nullptr;
    stats.lastError = registered;
    return false;
  }
  stats.active = stats.supported = true;
  return true;
#endif
}

bool tab5DisplayPresentReady() {
  if (!isOwner()) return false;
  if (!stats.active) return true;
  return !drawing && finishPending()
      && !(retryBlocked && static_cast<uint32_t>(nowMs() - retryAfter) < kRetryMs);
}

bool tab5DisplayPresentBeginFrame(Tab5DisplayMirrorSpan mirrored) {
  if (!isOwner()) return false;
  if (!stats.active) return true;
  if (drawing || !finishPending()
      || (retryBlocked && static_cast<uint32_t>(nowMs() - retryAfter) < kRetryMs)) {
    ++stats.skippedFrames;
    repaintNeeded = true;
    return false;
  }
  if (!selectBack()) {
    stats.lastError = ESP_ERR_INVALID_STATE;
    retryAfter = nowMs();
    retryBlocked = repaintNeeded = true;
    mirrorNeeded = fullMirror = true;
    ++stats.skippedFrames;
    return false;
  }
  deferredMirror = mirrorNeeded && pendingMirror && mirrored;
  mirrorRecorder = mirrored;
  if (deferredMirror) {
    for (uint32_t y = 0; y < geometry.height; ++y)
      Tab5DisplayPendingMirror::reset(pendingMirror[y], fullMirror ? 0 : dirty[y].first,
                                     fullMirror ? geometry.width : dirty[y].end);
    mirrorNeeded = fullMirror = false;
  } else if (mirrorNeeded && !mirrorFront(mirrored)) {
    ++stats.skippedFrames;
    repaintNeeded = true;
    return false;
  }
  retryBlocked = false;
  Ranges::clear(geometry, dirty);
  nativeTransferEnabled = mirrored != nullptr;
  drawing = true;
  return true;
}

void tab5DisplayPresentDirtyRect(int32_t x, int32_t y, int32_t width, int32_t height) {
  if (!isOwner() || !stats.active || !drawing) return;
  Ranges::Rect rect{};
  if (!Ranges::physicalRect(geometry, x, y, width, height, rect)) {
    fullMirror = true;
    return;
  }
  if (deferredMirror)
    for (uint32_t row = rect.y; row < uint32_t(rect.y) + rect.height; ++row)
      Tab5DisplayPendingMirror::overwrite(pendingMirror[row], rect.x, rect.x + rect.width,
          [row](uint16_t first, uint16_t end) { copyPendingSpan(uint16_t(row), first, end); });
  Ranges::addRect(geometry, dirty, x, y, width, height);
}

bool tab5DisplayPresentWritePixels(int32_t x, int32_t y, int32_t width, int32_t height,
                                  const uint8_t* pixels, size_t bytes, size_t sourceStride) {
  if (!isOwner() || !stats.active || !drawing || !nativeTransferEnabled || panel != M5.Display.getPanel()
      || M5.Display.getColorDepth() != 16 || M5.Display.getSwapBytes()) return false;
  const uint64_t started = static_cast<uint64_t>(esp_timer_get_time());
  if (!Tab5DisplayBlit::copy(buffers[stats.back], size_t(geometry.stride) * geometry.height,
                            geometry, x, y, width, height, pixels, bytes, sourceStride)) return false;
  stats.nativeBlitPixels += uint64_t(uint32_t(width)) * uint32_t(height);
  ++stats.nativeBlitRects;
  const uint64_t elapsed = static_cast<uint64_t>(esp_timer_get_time()) - started;
  if (elapsed > stats.maxNativeBlitUs)
    stats.maxNativeBlitUs = elapsed > UINT32_MAX ? UINT32_MAX : uint32_t(elapsed);
  return true;
}

bool tab5DisplayPresentPrepareFrame(bool trackingValid) {
  if (!isOwner()) return false;
  if (!stats.active || !drawing || !deferredMirror) return true;
  // Once an untracked write invalidates the announced-area contract, its pixels
  // cannot safely be distinguished from old content. Keep the front, then redraw.
  if (!trackingValid) return false;
  for (uint32_t y = 0; y < geometry.height; ++y)
    Tab5DisplayPendingMirror::flush(pendingMirror[y],
        [y](uint16_t first, uint16_t end) { copyPendingSpan(uint16_t(y), first, end); });
  deferredMirror = false;
  return true;
}

void tab5DisplayPresentEndFrame(bool cacheReady) {
  if (!isOwner() || !stats.active || !drawing) return;
  drawing = false;
  cacheReady = cacheReady && !deferredMirror;
  deferredMirror = false;
  if (!cacheReady) {
    repaintNeeded = mirrorNeeded = fullMirror = retryBlocked = true;
    retryAfter = nowMs();
    return;
  }
  // All dirty areas have already been written back by Tab5DisplayCache. With
  // an IDF-owned framebuffer, draw_bitmap selects the entire image regardless
  // of its rectangle. A valid first-row rectangle avoids a second full-screen
  // cache walk; no application pixels are copied by IDF on this path.
  const auto result = esp_lcd_panel_draw_bitmap(handle, 0, 0, 720, 1, buffers[stats.back]);
  if (result != ESP_OK) {
    ++stats.publishErrors;
    stats.lastError = result;
    repaintNeeded = true;
    mirrorNeeded = fullMirror = true;
    retryBlocked = true;
    retryAfter = nowMs();
    return;
  }
  submittedAfter = refreshes.load(std::memory_order_acquire);
  submittedAt = nowMs();
  stats.pending = true;
  ++stats.submissions;
}

bool tab5DisplayPresentNeedsRedraw() {
  if (!isOwner() || !stats.active || !repaintNeeded || drawing) return false;
  if (!finishPending()
      || (retryBlocked && static_cast<uint32_t>(nowMs() - retryAfter) < kRetryMs)) return false;
  repaintNeeded = false;
  return true;
}

bool tab5DisplayPresentOwnsBuffer(uint32_t address) {
  if (!isOwner() || !stats.active) return false;
  return address == uint32_t(reinterpret_cast<uintptr_t>(buffers[0]))
      || address == uint32_t(reinterpret_cast<uintptr_t>(buffers[1]));
}

Tab5DisplayPresentStats tab5DisplayPresentStats() {
  if (!isOwner()) return {};
  auto result = stats;
  result.refreshEvents = refreshes.load(std::memory_order_acquire);
  result.maxRefreshGapUs = maxRefreshGapUs.load(std::memory_order_relaxed);
  return result;
}

void tab5DisplayPresentDumpConfiguration(
    void (*emit)(const char*, size_t, void*), void* context) {
  if (!emit) return;
  auto* candidate = isOwner() && M5.getBoard() == m5::board_t::board_M5Tab5
      ? M5.Display.getPanel() : nullptr;
  if (!candidate ||
      !candidate->getBus() || candidate->getBus()->busType() != lgfx::bus_type_t::bus_dsi) {
    static constexpr char unavailable[] = "[DISPLAY config] available=0\n";
    emit(unavailable, sizeof(unavailable) - 1, context);
    return;
  }
  const auto& cfg = candidate->config();
  const auto detail = static_cast<lgfx::Panel_DSI*>(candidate)->config_detail();
  char line[320];
  const int written = snprintf(line, sizeof(line),
      "[DISPLAY config] available=1 requestedPixelClockMHz=%u width=%u height=%u rotation=%u offsetRotation=%u hBack=%u hPulse=%u hFront=%u vBack=%u vPulse=%u vFront=%u\n",
      unsigned(detail.dpi_freq_mhz), unsigned(cfg.panel_width), unsigned(cfg.panel_height),
      unsigned(candidate->getRotation()), unsigned(cfg.offset_rotation),
      unsigned(detail.hsync_back_porch), unsigned(detail.hsync_pulse_width),
      unsigned(detail.hsync_front_porch), unsigned(detail.vsync_back_porch),
      unsigned(detail.vsync_pulse_width), unsigned(detail.vsync_front_porch));
  if (written > 0 && size_t(written) < sizeof(line))
    emit(line, size_t(written), context);
}

#else
bool tab5DisplayPresentInit() { return false; }
bool tab5DisplayPresentReady() { return true; }
bool tab5DisplayPresentBeginFrame(Tab5DisplayMirrorSpan mirrored) { return true; }
void tab5DisplayPresentDirtyRect(int32_t, int32_t, int32_t, int32_t) {}
bool tab5DisplayPresentWritePixels(int32_t, int32_t, int32_t, int32_t, const uint8_t*, size_t, size_t) { return false; }
bool tab5DisplayPresentPrepareFrame(bool) { return true; }
void tab5DisplayPresentEndFrame(bool) {}
bool tab5DisplayPresentNeedsRedraw() { return false; }
bool tab5DisplayPresentOwnsBuffer(uint32_t) { return false; }
Tab5DisplayPresentStats tab5DisplayPresentStats() { return {}; }
void tab5DisplayPresentDumpConfiguration(
    void (*emit)(const char*, size_t, void*), void* context) {
  if (!emit) return;
  static constexpr char unavailable[] = "[DISPLAY config] available=0\n";
  emit(unavailable, sizeof(unavailable) - 1, context);
}
#endif
