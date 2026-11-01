#pragma once
#include <stdint.h>

namespace Tab5DisplayOrientation {
// M5GFX panel rotations: keep landscape/portrait dimensions, reverse both axes.
constexpr uint8_t rotation(bool portrait, bool upsideDown) {
  return static_cast<uint8_t>(((portrait ? 2u : 3u) + (upsideDown ? 2u : 0u)) & 3u);
}
}
