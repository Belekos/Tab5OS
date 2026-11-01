#pragma once
#include <algorithm>
#include <cstdint>

// Display zoom and downloaded XYZ zoom are separate. Legacy style.maxZoom
// includes zoomShift (osm-fast: max 20, shift -1 => native maximum 19).
namespace Tab5MapViewPolicy {
constexpr uint8_t MaxZoom = 22;
constexpr uint8_t MaxOverzoom = 5;
inline uint8_t nativeMax(uint8_t maximum, int8_t shift) {
  return static_cast<uint8_t>(std::max(0, std::min(int(MaxZoom), int(maximum) + int(shift))));
}
inline uint8_t displayMax(uint8_t maximum, int8_t shift) {
  return std::min<unsigned>(MaxZoom, nativeMax(maximum, shift) + MaxOverzoom);
}
inline uint8_t sourceZoom(uint8_t view, uint8_t maximum, int8_t shift) {
  return static_cast<uint8_t>(std::max(0, std::min(int(nativeMax(maximum, shift)), int(view) + int(shift))));
}
inline unsigned tilePixels(uint8_t view, uint8_t source) {
  return 256u << std::max(0, std::min(int(MaxOverzoom), int(view) - int(source)));
}
}
