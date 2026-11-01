#include "Tab5ImageDecode.h"
#include "Tab5WebPPort.h"
#include "Tab5Cooperate.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <lgfx/utility/lgfx_tjpgd.h>
#include "src/tab5png/tab5_pngle.h"
#ifdef ARDUINO_ARCH_ESP32
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
namespace Tab5ImageDecode {
namespace {
constexpr unsigned MaxDimension = 20000;
constexpr size_t MaxSource = 12U * 1024U * 1024U;
void* allocate(size_t bytes) {
#ifdef ARDUINO_ARCH_ESP32
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return std::malloc(bytes);
#endif
}
void release(void* bytes) {
#ifdef ARDUINO_ARCH_ESP32
  heap_caps_free(bytes);
#else
  std::free(bytes);
#endif
}
struct Bytes {
  void* data;
  explicit Bytes(size_t bytes) : data(allocate(bytes)) {}
  ~Bytes() { release(data); }
};
struct Context {
  const Source& source; Image& image; Callbacks callbacks;
  size_t position = 0, pixelsFilled = 0;
  unsigned lastProgress = 0, pngShift = 0;
  bool stopped = false, invalid = false;
  Tab5TimeSlice slice;
  bool cooperate() {
    if (callbacks.cooperate) callbacks.cooperate();
#ifdef ARDUINO_ARCH_ESP32
    else if (slice.due(millis())) vTaskDelay(1);
#endif
    if (callbacks.cancelled && callbacks.cancelled()) stopped = true;
    return !stopped;
  }
  void allocated() {
    if (callbacks.allocated) callbacks.allocated(callbacks.allocationContext, size_t(image.width) * image.height * 2);
  }
  void progress(unsigned completed, unsigned total) {
    if (!total || !callbacks.progress) return;
    unsigned value = std::min(99U, unsigned(uint64_t(completed) * 99 / total));
    if (value > lastProgress) { lastProgress = value; callbacks.progress(value); }
  }
  bool seek(size_t at) {
    if (at > source.size || !source.seek(source.context, at)) return false;
    position = at; return true;
  }
  static uint32_t read(void* opaque, uint8_t* bytes, uint32_t count) {
    auto& c = *static_cast<Context*>(opaque);
    if (!c.cooperate()) return 0;
    if (!bytes) return count <= c.source.size - c.position && c.seek(c.position + count) ? count : 0;
    count = uint32_t(std::min<size_t>(count, c.source.size - c.position));
    const size_t got = c.source.read(c.source.context, bytes, count);
    if (got > count) { c.invalid = true; return 0; }
    c.position += got;
    return uint32_t(got);
  }
};
uint16_t rgb565(unsigned r, unsigned g, unsigned b) {
  return uint16_t((r & 0xf8) << 8 | (g & 0xfc) << 3 | b >> 3);
}
bool dimensions(unsigned w, unsigned h) {
  return w && h && w <= MaxDimension && h <= MaxDimension;
}
bool create(Image& image, unsigned w, unsigned h, size_t limit) {
  if (!w || !h || size_t(w) > limit / h || size_t(w) > SIZE_MAX / 2 / h) return false;
  image.pixels = static_cast<uint16_t*>(allocate(size_t(w) * h * 2));
  if (!image.pixels) return false;
  image.width = w; image.height = h;
  return true;
}
uint32_t jpegBlock(void* opaque, void* bitmap, JRECT* rect) {
  auto& c = *static_cast<Context*>(opaque);
  if (!c.cooperate()) return 0;
  if (!bitmap || rect->right < rect->left || rect->bottom < rect->top ||
      rect->right >= c.image.width || rect->bottom >= c.image.height) { c.invalid = true; return 0; }
  const auto* rgb = static_cast<const uint8_t*>(bitmap);
  for (unsigned y = rect->top; y <= rect->bottom; ++y) {
    auto* out = c.image.pixels + size_t(y) * c.image.width + rect->left;
    for (unsigned x = rect->left; x <= rect->right; ++x, rgb += 3) *out++ = rgb565(rgb[0], rgb[1], rgb[2]);
  }
  c.pixelsFilled += size_t(rect->right - rect->left + 1) * (rect->bottom - rect->top + 1);
  c.progress(rect->bottom + 1, c.image.height);
  return 1;
}
void pngBlock(void* opaque, uint32_t x, uint32_t y, uint_fast8_t step, size_t count, const uint8_t* argb) {
  auto& c = *static_cast<Context*>(opaque);
  if (!c.cooperate()) return; // Input callback aborts at the next short read.
  if (!argb || !step || y >= c.image.sourceHeight || x >= c.image.sourceWidth ||
      !count || count - 1 > (c.image.sourceWidth - 1 - x) / step) { c.invalid = true; c.stopped = true; return; }
  const unsigned mask = (1U << c.pngShift) - 1;
  if (y & mask) return;
  const unsigned dy = y >> c.pngShift;
  for (size_t i = 0; i < count; ++i, x += step, argb += 4) {
    if (x & mask) continue;
    const unsigned dx = x >> c.pngShift;
    if (dx >= c.image.width || dy >= c.image.height) continue;
    ++c.pixelsFilled;
    const unsigned alpha = argb[0], inverse = 255 - alpha;
    c.image.pixels[size_t(dy) * c.image.width + dx] = rgb565(
        (argb[1] * alpha + 255 * inverse + 127) / 255,
        (argb[2] * alpha + 255 * inverse + 127) / 255,
        (argb[3] * alpha + 255 * inverse + 127) / 255);
  }
  c.progress(y + 1, c.image.sourceHeight);
}
unsigned big32(const uint8_t* bytes) {
  return unsigned(bytes[0]) << 24 | unsigned(bytes[1]) << 16 | unsigned(bytes[2]) << 8 | bytes[3];
}
}
Image::~Image() { clear(); }
void Image::clear() { release(pixels); pixels = nullptr; width = height = sourceWidth = sourceHeight = 0; }

bool load(const Source& source, Image& image, const char*& error, Callbacks callbacks, size_t maxPixels) {
  image.clear(); error = nullptr;
  if (!source.read || !source.seek || source.size < 24 || source.size > MaxSource || !maxPixels) {
    error = "Image inaccessible ou trop volumineuse"; return false;
  }
  Context c{source, image, callbacks}; uint8_t header[29]{};
  if (!c.seek(0) || Context::read(&c, header, 24) != 24 || !c.seek(0)) {
    error = c.stopped ? "Préparation annulée" : "Lecture de l’image impossible"; return false;
  }
  if (!memcmp(header, "RIFF", 4) && !memcmp(header + 8, "WEBP", 4))
    return Tab5WebP::load(source, image, error, callbacks, maxPixels);
  bool ok = false;
  if (header[0] == 0xff && header[1] == 0xd8) {
    Bytes pool(4096); lgfxJdec decoder{};
    if (!pool.data) error = "PSRAM insuffisante pour le décodeur JPEG";
    else {
      const auto prepared = lgfx_jd_prepare(&decoder, Context::read, pool.data, 4096, &c);
      if (prepared != JDR_OK) error = prepared == JDR_FMT3 ? "JPEG non pris en charge (utiliser un JPEG standard)" : "JPEG incomplet ou invalide";
      else if (!dimensions(decoder.width, decoder.height)) error = "Dimensions JPEG invalides";
      else {
        image.sourceWidth = decoder.width; image.sourceHeight = decoder.height;
        unsigned shift = 0, w = decoder.width, h = decoder.height;
        // Round down exactly as TJpgDec does at the last MCU. This prevents an
        // unfilled row/column for odd-sized JPEGs, and never stretches a crop.
        while (shift < 3 && size_t(w) > maxPixels / h) { ++shift; w = decoder.width >> shift; h = decoder.height >> shift; if (!w || !h) break; }
        if (!create(image, w, h, maxPixels)) error = "PSRAM insuffisante pour préparer l’image";
        else {
          c.allocated();
          const auto decoded = lgfx_jd_decomp(&decoder, jpegBlock, shift);
          ok = decoded == JDR_OK && !c.stopped && !c.invalid &&
               c.pixelsFilled == size_t(image.width) * image.height;
          if (!ok) error = "Décodage JPEG interrompu ou image incomplète";
        }
      }
    }
  } else if (!memcmp(header, "\x89PNG\r\n\x1a\n", 8)) {
    if (Context::read(&c, header, 29) != 29 || !c.seek(0)) {
      error = c.stopped ? "Préparation annulée" : "En-tête PNG incomplet"; return false;
    }
    const unsigned depth = header[24], color = header[25];
    const bool validDepth = color == 0 ? (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16) :
                            color == 3 ? (depth == 1 || depth == 2 || depth == 4 || depth == 8) :
                            (color == 2 || color == 4 || color == 6) && (depth == 8 || depth == 16);
    if (!validDepth || header[26] || header[27] || header[28] > 1) {
      error = "En-tête PNG invalide"; return false;
    }
    image.sourceWidth = big32(header + 16); image.sourceHeight = big32(header + 20);
    if (!dimensions(image.sourceWidth, image.sourceHeight)) error = "Dimensions PNG invalides";
    else {
      unsigned w = image.sourceWidth, h = image.sourceHeight;
      while (size_t(w) > maxPixels / h) { ++c.pngShift; w = (image.sourceWidth + (1U << c.pngShift) - 1) >> c.pngShift; h = (image.sourceHeight + (1U << c.pngShift) - 1) >> c.pngShift; }
      if (!create(image, w, h, maxPixels)) error = "PSRAM insuffisante pour préparer l’image";
      else {
        c.allocated();
        // Transparent pixels are composited onto paper white, without any
        // readback or DMA allocation from a display/canvas.
        std::memset(image.pixels, 255, size_t(w) * h * 2);
        tab5_pngle_t* decoder = tab5_pngle_new();
        if (!decoder) error = "PSRAM insuffisante pour le décodeur PNG";
        else {
          if (tab5_pngle_prepare(decoder, Context::read, &c) == 0 &&
              tab5_pngle_get_width(decoder) == image.sourceWidth && tab5_pngle_get_height(decoder) == image.sourceHeight)
            ok = tab5_pngle_decomp(decoder, pngBlock) == 0 && !c.stopped && !c.invalid &&
                 c.pixelsFilled == size_t(image.width) * image.height;
          tab5_pngle_destroy(decoder);
          if (!ok) error = "Décodage PNG interrompu ou image incomplète";
        }
      }
    }
  } else error = "Format d’image non reconnu";
  if (!ok) { if (c.stopped && !c.invalid) error = "Préparation annulée"; image.clear(); }
  else if (callbacks.progress) callbacks.progress(100);
  return ok;
}
#ifdef ARDUINO
bool load(fs::File& file, Image& image, String& error, Callbacks callbacks, size_t maxPixels) {
  Source source{&file, file.size(),
    [](void* p, uint8_t* bytes, size_t count) { return static_cast<fs::File*>(p)->read(bytes, count); },
    [](void* p, size_t position) { return static_cast<fs::File*>(p)->seek(position); }};
  const char* reason = nullptr; const bool result = load(source, image, reason, callbacks, maxPixels);
  error = reason ? reason : ""; return result;
}
#endif
}
