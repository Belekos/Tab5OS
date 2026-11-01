#include "Tab5MemoryLayout.h"
#include <sdkconfig.h>
#include <esp_heap_caps.h>
#include <stdint.h>

namespace Tab5MemoryLayout {
namespace {
Pool read(uint32_t caps, bool largest) {
  Pool p;
  p.total=heap_caps_get_total_size(caps);
  p.free=heap_caps_get_free_size(caps);
  p.minimumFree=heap_caps_get_minimum_free_size(caps);
  if(largest)p.largestFree=heap_caps_get_largest_free_block(caps);
  return p;
}
}
Snapshot sample(bool includeLargest) {
  Snapshot s;
  s.largestSampled=includeLargest;
  s.internal=read(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,includeLargest);
  s.psram=read(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT,includeLargest);
#if defined(CONFIG_IDF_TARGET_ESP32P4) && CONFIG_IDF_TARGET_ESP32P4
  // Pinned ESP-IDF 5.5.4 components/heap/port/esp32p4/memory_layout.c:
  // both HP L2MEM types have INTERNAL|DMA, while TCM and RTC/LP do not.
  // INTERNAL also excludes any external memory with DMA capabilities.
  // This partitions the registered heaps directly, without subtracting blocks.
  s.hp=read(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT,includeLargest);
  s.tcm=read(MALLOC_CAP_TCM|MALLOC_CAP_8BIT,includeLargest);
  s.lp=read(MALLOC_CAP_RTCRAM|MALLOC_CAP_8BIT,includeLargest);
  s.hpPhysicalBytes=768U*1024U;
  s.tcmPhysicalBytes=8U*1024U; // SPM is the current name for this same TCM bank.
  s.lpPhysicalBytes=32U*1024U;
  s.splitAvailable=true;
#if defined(CONFIG_CACHE_L2_CACHE_SIZE)
  s.l2CacheBytes=CONFIG_CACHE_L2_CACHE_SIZE;
#endif
#endif
  return s;
}
}
