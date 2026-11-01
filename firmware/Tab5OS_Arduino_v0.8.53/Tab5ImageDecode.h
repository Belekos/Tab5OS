#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef ARDUINO
#include <Arduino.h>
#include <FS.h>
#endif

// CPU-only image preparation. Decoder state and pixels never borrow the live
// display or DMA buffers. JPEG output uses native 1/2, 1/4 or 1/8 reduction;
// PNG is sampled while rows are decoded, including transparent/interlaced PNG.
namespace Tab5ImageDecode {
struct Source {
  void* context = nullptr;
  size_t size = 0;
  size_t (*read)(void*, uint8_t*, size_t) = nullptr;
  bool (*seek)(void*, size_t) = nullptr;
};
struct Callbacks {
  bool (*cancelled)() = nullptr;
  void (*progress)(unsigned) = nullptr;
  // Optional host/test hook; ESP32 otherwise yields at least every 4 ms.
  void (*cooperate)() = nullptr;
  void* allocationContext = nullptr;
  // Let the OS subtract memory now present in the real heap sample from its
  // pending reservation, rather than counting the same pixels twice.
  void (*allocated)(void*, size_t) = nullptr;
};
class Image {
 public:
  Image() = default;
  ~Image();
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;
  void clear();
  uint16_t* pixels = nullptr; // Native RGB565 words, not byte-swapped.
  unsigned width = 0, height = 0, sourceWidth = 0, sourceHeight = 0;
};
bool load(const Source& source, Image& image, const char*& error,
          Callbacks callbacks = {}, size_t maxPixels = 4U * 1024U * 1024U);
#ifdef ARDUINO
bool load(fs::File& file, Image& image, String& error,
          Callbacks callbacks = {}, size_t maxPixels = 4U * 1024U * 1024U);
#endif
}
