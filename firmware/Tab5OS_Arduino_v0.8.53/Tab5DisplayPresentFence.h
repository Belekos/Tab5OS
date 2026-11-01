#pragma once
#include <stdint.h>

namespace Tab5DisplayPresentFence {
// Capture after esp_lcd_panel_draw_bitmap returns. An interrupt before/during
// publication cannot release the old front. Two later completions cover either
// DMA completion (P4 rev < 3) or bridge VSYNC (rev >= 3), including ISR ordering.
inline bool released(uint32_t current, uint32_t afterPublication) {
  return static_cast<uint32_t>(current - afterPublication) >= 2;
}
}
