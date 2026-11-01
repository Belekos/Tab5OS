#pragma once
#include <stdint.h>

struct Tab5DisplayCacheStats {
  uint64_t requestedBytes = 0;
  uint64_t syncedBytes = 0;
  uint32_t frames = 0;
  uint32_t filteredFrames = 0;
  uint32_t fallbackFrames = 0;
  uint32_t cacheCalls = 0;
  uint32_t cacheErrors = 0;
  int32_t lastCacheError = 0;
  uint32_t recoveryRequests = 0;
  uint32_t recoveryAttempts = 0;
  uint32_t recoveries = 0;
  uint32_t lastRequestedBytes = 0;
  uint32_t lastSyncedBytes = 0;
  uint32_t lastCacheCalls = 0;
  bool configured = false;
  bool pendingRecovery = false;
};

// Call after M5.begin, from the same UI task that owns displayFlush. Allocation
// is PSRAM-only; failure leaves the original chunked writeback available.
bool tab5DisplayCacheInit();

// Scope exactly one LVGL frame transaction. Record every rectangle BEFORE its
// synchronous M5.Display.pushImage. End AFTER M5.Display.endWrite. With the
// double-buffer presenter, End synchronizes mirrored and drawn pixels together
// before publication; outside that path the generic writeback stays immediate.
// Other direct M5 drawing must remain outside this scope.
// False means no safe writable framebuffer is available. The caller must skip
// every flush chunk of this LVGL frame, while still calling flush_ready.
bool tab5DisplayCacheBeginFrame();
void tab5DisplayCacheDirtyRect(int32_t x, int32_t y, int32_t width, int32_t height);
void tab5DisplayCacheEndFrame();
// If true, invalidate the active LVGL screen after lv_timer_handler. Cache-error
// recovery requests and full-buffer retries are paced at most once per second.
bool tab5DisplayCacheNeedsRedraw();

// UI-task snapshot. Counts cache-maintenance traffic, not LCD/PSRAM bus traffic.
Tab5DisplayCacheStats tab5DisplayCacheStats();
