#pragma once
#include <stdint.h>
#include <stddef.h>

struct Tab5DisplayPresentStats {
  uint64_t mirroredBytes = 0;
  uint64_t nativeBlitPixels = 0;
  uint32_t nativeBlitRects = 0;
  uint32_t maxNativeBlitUs = 0;
  uint32_t refreshEvents = 0;
  uint32_t maxRefreshGapUs = 0;
  uint32_t submissions = 0;
  uint32_t flips = 0;
  uint32_t timeouts = 0;
  uint32_t skippedFrames = 0;
  uint32_t copyErrors = 0;
  uint32_t publishErrors = 0;
  uint32_t maxWaitMs = 0; // Maximum observed pending-fence age; never a blocking wait.
  int32_t lastError = 0;
  uint8_t front = 0;
  uint8_t back = 1;
  bool supported = false;
  bool active = false;
  bool pending = false;
  bool waiting = false;
};

// UI-task only. Uses the two existing M5GFX framebuffers, never a third image.
bool tab5DisplayPresentInit();
// Poll only: never sleeps, allocates, copies pixels or touches scanning memory.
bool tab5DisplayPresentReady();
// Optional recorder lets the cache owner combine mirror and draw writeback.
// Receives physical pixel spans after each mirror copy; no ownership transfer.
using Tab5DisplayMirrorSpan = void (*)(uint16_t row, uint16_t first, uint16_t end);
bool tab5DisplayPresentBeginFrame(Tab5DisplayMirrorSpan mirrored = nullptr);
void tab5DisplayPresentDirtyRect(int32_t x, int32_t y, int32_t width, int32_t height);
// Call after DirtyRect, before EndFrame. False leaves pixels untouched and
// selects the existing M5GFX path. Writes only the non-scanning back buffer.
bool tab5DisplayPresentWritePixels(int32_t x, int32_t y, int32_t width, int32_t height,
                                  const uint8_t* pixels, size_t bytes, size_t sourceStride = 0);
// Finish deferred old spans before cache writeback. The caller must announce
// each write through DirtyRect before changing pixels, then supply trackingValid.
bool tab5DisplayPresentPrepareFrame(bool trackingValid);
void tab5DisplayPresentEndFrame(bool cacheReady);
bool tab5DisplayPresentNeedsRedraw();
bool tab5DisplayPresentOwnsBuffer(uint32_t address);
Tab5DisplayPresentStats tab5DisplayPresentStats();
// UI-task diagnostic only. Reads configuration without I/O, allocation or reset.
void tab5DisplayPresentDumpConfiguration(
    void (*emit)(const char*, size_t, void*), void* context = nullptr);
