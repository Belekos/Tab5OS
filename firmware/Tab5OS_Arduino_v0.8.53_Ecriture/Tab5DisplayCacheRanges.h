#pragma once
#include <stdint.h>

// Pure range arithmetic, kept independent of M5GFX/ESP-IDF for coverage tests.
namespace Tab5DisplayCacheRanges {
constexpr uint32_t kAlignment = 128;
constexpr uint32_t kChunkBytes = 16 * 1024;
struct Row { uint16_t first; uint16_t end; }; // End is exclusive, physical pixels.
struct Geometry {
  uint32_t base, end, stride;
  uint16_t width, height, logicalWidth, logicalHeight;
  uint8_t rotation;
};

inline bool alignedRange(uint32_t addr, uint32_t size, uint32_t& first,
                         uint32_t& end) {
  const uint64_t a = uint64_t(addr) & ~(uint64_t(kAlignment) - 1);
  const uint64_t b = (uint64_t(addr) + size + kAlignment - 1)
                   & ~(uint64_t(kAlignment) - 1);
  if (b > UINT32_MAX) return false;
  first = uint32_t(a);
  end = uint32_t(b);
  return true;
}

inline bool configure(Geometry& g, uint32_t base, uint16_t width,
                      uint16_t height, uint32_t stride, uint8_t rotation,
                      uint8_t offsetRotation) {
  if (!base || !width || !height || stride < uint32_t(width) * 2
      || (stride & 3) || rotation > 7 || offsetRotation > 7) return false;
  const uint64_t end = uint64_t(base) + uint64_t(stride) * height;
  uint32_t firstAligned, endAligned;
  if (end > UINT32_MAX
      || !alignedRange(base, uint32_t(end - base), firstAligned, endAligned)) {
    return false;
  }
  const uint8_t r = ((rotation + offsetRotation) & 3)
                  | ((rotation & 4) ^ (offsetRotation & 4));
  g = {base, uint32_t(end), stride, width, height,
       (r & 1) ? height : width, (r & 1) ? width : height, r};
  return true;
}

inline void clear(const Geometry& g, Row* rows) {
  for (uint32_t y = 0; y < g.height; ++y) rows[y] = {UINT16_MAX, 0};
}

struct Rect { uint16_t x, y, width, height; };
inline bool physicalRect(const Geometry& g, int32_t x, int32_t y,
                         int32_t width, int32_t height, Rect& result) {
  result = {};
  if (width <= 0 || height <= 0) return false;
  int64_t x0 = x, y0 = y;
  int64_t x1 = x0 + int64_t(width), y1 = y0 + int64_t(height);
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > g.logicalWidth) x1 = g.logicalWidth;
  if (y1 > g.logicalHeight) y1 = g.logicalHeight;
  if (x0 >= x1 || y0 >= y1) return true;
  int32_t px = int32_t(x0), py = int32_t(y0);
  int32_t w = int32_t(x1 - x0), h = int32_t(y1 - y0);
  // Match Panel_FrameBufferBase::_rotate_pixelcopy, including mirror modes.
  if ((1u << g.rotation) & 0b10010110u) py = g.logicalHeight - (py + h);
  if (g.rotation & 2) px = g.logicalWidth - (px + w);
  if (g.rotation & 1) {
    const int32_t t = px; px = py; py = t;
    const int32_t s = w; w = h; h = s;
  }
  if (px < 0 || py < 0 || px + w > g.width || py + h > g.height) return false;
  result = {uint16_t(px), uint16_t(py), uint16_t(w), uint16_t(h)};
  return true;
}

inline bool addRect(const Geometry& g, Row* rows, int32_t x, int32_t y,
                    int32_t width, int32_t height) {
  if (!rows) return false;
  Rect r{};
  if (!physicalRect(g, x, y, width, height, r)) return false;
  for (uint32_t row = r.y; row < uint32_t(r.y) + r.height; ++row) {
    if (rows[row].first > r.x) rows[row].first = r.x;
    if (rows[row].end < uint32_t(r.x) + r.width) rows[row].end = r.x + r.width;
  }
  return true;
}

// first/end are the aligned range requested by M5GFX. Never expand beyond it.
// Adjacent/overlapping cache-line ranges merge; untouched gaps stay omitted.
// emit returns false on a cache API error so the caller can propagate it.
template <typename Emit>
bool emitRanges(const Geometry& g, const Row* rows, uint32_t first,
                uint32_t end, Emit emit) {
  uint32_t pendingFirst = 0, pendingEnd = 0;
  for (uint32_t y = 0; y < g.height; ++y) {
    if (rows[y].first >= rows[y].end) continue;
    uint32_t a, b;
    if (!alignedRange(g.base + y * g.stride + uint32_t(rows[y].first) * 2,
                      uint32_t(rows[y].end - rows[y].first) * 2, a, b)) return false;
    if (a < first) a = first;
    if (b > end) b = end;
    if (a >= b) continue;
    if (pendingEnd && a > pendingEnd) {
      if (!emit(pendingFirst, pendingEnd)) return false;
      pendingEnd = 0;
    }
    if (!pendingEnd) pendingFirst = a;
    if (b > pendingEnd) pendingEnd = b;
  }
  return !pendingEnd || emit(pendingFirst, pendingEnd);
}
}  // namespace Tab5DisplayCacheRanges
