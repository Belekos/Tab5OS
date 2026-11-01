#pragma once
#include <stdint.h>
#include <stddef.h>
#include "Tab5DisplayCacheRanges.h"

namespace Tab5DisplayBlit {
// LVGL supplies big-endian RGB565; the DSI framebuffers hold native RGB565.
// Blocking keeps the source and destination working sets small during a 90°
// transpose. The only scratch storage is this function's 512-byte stack tile.
inline bool copy(uint8_t* destination, size_t destinationBytes,
                 const Tab5DisplayCacheRanges::Geometry& geometry,
                 int32_t x, int32_t y, int32_t width, int32_t height,
                 const uint8_t* source, size_t sourceBytes, size_t sourceStride = 0) {
  if (!sourceStride && width > 0) sourceStride = size_t(width) * 2;
  if (!destination || !source || (reinterpret_cast<uintptr_t>(destination) & 1)
      || width <= 0 || height <= 0 || x < 0 || y < 0
      || geometry.rotation > 7 || (geometry.stride & 1)
      || geometry.stride < uint32_t(geometry.width) * 2
      || int64_t(x) + width > geometry.logicalWidth
      || int64_t(y) + height > geometry.logicalHeight
      || uint64_t(geometry.stride) * geometry.height > destinationBytes
      || sourceStride < uint64_t(uint32_t(width)) * 2
      || sourceBytes < uint64_t(uint32_t(width)) * 2
      || (height > 1 && sourceStride > (sourceBytes - size_t(width) * 2) / size_t(height - 1))) return false;
  const bool transpose = geometry.rotation & 1;
  if (geometry.logicalWidth != (transpose ? geometry.height : geometry.width)
      || geometry.logicalHeight != (transpose ? geometry.width : geometry.height)) return false;
  const bool reverseX = geometry.rotation & 2;
  const bool reverseY = (1u << geometry.rotation) & 0b10010110u;
  constexpr int32_t side = 16;
  uint16_t tile[side * side];
  for (int32_t top = 0; top < height; top += side) {
    const int32_t rows = height - top < side ? height - top : side;
    for (int32_t left = 0; left < width; left += side) {
      const int32_t columns = width - left < side ? width - left : side;
      for (int32_t row = 0; row < rows; ++row) {
        const auto* input = source + size_t(top + row) * sourceStride + size_t(left) * 2;
        for (int32_t column = 0; column < columns; ++column)
          tile[row * side + column] = uint16_t(uint16_t(input[column * 2]) << 8)
                                    | uint16_t(input[column * 2 + 1]);
      }
      if (transpose) {
        for (int32_t column = 0; column < columns; ++column) {
          const int32_t py = reverseX ? geometry.logicalWidth - 1 - (x + left + column)
                                     : x + left + column;
          auto* output = reinterpret_cast<uint16_t*>(destination + size_t(py) * geometry.stride);
          for (int32_t row = 0; row < rows; ++row) {
            const int32_t px = reverseY ? geometry.logicalHeight - 1 - (y + top + row)
                                       : y + top + row;
            output[px] = tile[row * side + column];
          }
        }
      } else {
        for (int32_t row = 0; row < rows; ++row) {
          const int32_t py = reverseY ? geometry.logicalHeight - 1 - (y + top + row)
                                     : y + top + row;
          auto* output = reinterpret_cast<uint16_t*>(destination + size_t(py) * geometry.stride);
          for (int32_t column = 0; column < columns; ++column) {
            const int32_t px = reverseX ? geometry.logicalWidth - 1 - (x + left + column)
                                       : x + left + column;
            output[px] = tile[row * side + column];
          }
        }
      }
    }
  }
  return true;
}
}
