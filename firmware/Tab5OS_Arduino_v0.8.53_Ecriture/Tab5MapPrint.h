#pragma once
#include <Arduino.h>
struct Tab5MapTileResult;
namespace Tab5MapPrint {
// Existing swap565 canvas; caller retains it until done. No tile download.
bool beginCapture(const uint16_t* pixels,unsigned width,unsigned height,unsigned zoom,uint32_t generation,const String& credit);
void tileResult(const Tab5MapTileResult& result);
unsigned fallbackTileCount();
unsigned progress();
bool beginJpeg(double x,double y,unsigned viewZoom,unsigned maxZoom,uint32_t generation,const String& attribution);
bool begin(double centerX,double centerY,unsigned viewZoom,unsigned maxZoom,uint32_t generation,unsigned paper,unsigned dpi,bool landscape,const String& attribution,unsigned scalePercent=100);
void service(uint32_t generation,bool mapActive);
bool busy();const char* status();String detail();String takePath();void cancel();
}
