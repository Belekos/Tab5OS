#pragma once
#include "Tab5ImageDecode.h"
namespace Tab5WebP {
// Independent CPU worker. 12 MiB includes encoded bytes, decoder work,
// scaled RGBA and final RGB565; every allocation is strictly PSRAM on ESP32.
bool load(const Tab5ImageDecode::Source&, Tab5ImageDecode::Image&,
          const char*& error, Tab5ImageDecode::Callbacks, size_t maxPixels);
#ifdef TAB5_WEBP_TEST
void failAllocationAt(size_t index);
size_t allocationCount();
size_t liveBytes();
size_t peakBytes();
#endif
}
