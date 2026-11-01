#pragma once
#include <Arduino.h>
// Reads RGB565 little-endian rows and writes a complete JPEG before publishing its name.
bool tab5SaveJpeg(const uint16_t* pixels, unsigned width, unsigned height,
                  const String& destination, String& error);

namespace Tab5Jpeg {bool start(const String& raw,const String& destination);bool busy();String result();String error();void cancel();unsigned progress();}
