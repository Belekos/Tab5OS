#pragma once
#include "Tab5TileValidation.h"
namespace Tab5TileRaster {
// Destination is the M5Canvas byte-swapped RGB565 buffer. Source is decoded
// into at most 128 KiB of PSRAM before any destination pixels are changed.
bool draw(uint16_t* destination, unsigned width, unsigned height,
          const uint8_t* data, size_t size, int x, int y, int tileSize,
          int clipX=0, int clipY=0, int clipWidth=-1, int clipHeight=-1,
          Tab5TileValidation::Result* result=nullptr);
}
