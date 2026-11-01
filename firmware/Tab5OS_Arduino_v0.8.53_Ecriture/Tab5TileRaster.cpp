#include "Tab5TileRaster.h"
#include "Tab5Cooperate.h"
#include <algorithm>
#include <cstdlib>
#ifdef ARDUINO_ARCH_ESP32
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
namespace Tab5TileRaster {
bool draw(uint16_t* destination,unsigned width,unsigned height,const uint8_t* data,size_t size,
          int x,int y,int tileSize,int clipX,int clipY,int clipWidth,int clipHeight,
          Tab5TileValidation::Result* result) {
 using Tab5TileValidation::Result;
 if (result) *result=Result::Unavailable;
 if (!destination || !width || !height || width>32768 || height>32768 || tileSize<=0 || tileSize>1048576) return false;
#ifdef ARDUINO_ARCH_ESP32
 auto* pixels=static_cast<uint16_t*>(heap_caps_malloc(256U*256U*2U,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
 auto* pixels=static_cast<uint16_t*>(std::malloc(256U*256U*2U));
#endif
 if (!pixels) {if(result)*result=Result::NoMemory;return false;}
 const auto decoded=Tab5TileValidation::decode(data,size,pixels);
 if (result) *result=decoded;
 if (decoded==Result::Valid) {
  const int64_t left=std::max({int64_t(0),int64_t(x),int64_t(clipX)});
  const int64_t top=std::max({int64_t(0),int64_t(y),int64_t(clipY)});
  const int64_t right=std::min({int64_t(width),int64_t(x)+tileSize,clipWidth<0?int64_t(width):int64_t(clipX)+clipWidth});
  const int64_t bottom=std::min({int64_t(height),int64_t(y)+tileSize,clipHeight<0?int64_t(height):int64_t(clipY)+clipHeight});
  Tab5TimeSlice slice;
  for (int64_t dy=top;dy<bottom;++dy) {
   const unsigned sy=unsigned((dy-y)*256/tileSize);
   for (int64_t dx=left;dx<right;++dx) {
    const uint16_t value=pixels[size_t(sy)*256+unsigned((dx-x)*256/tileSize)];
    destination[size_t(dy)*width+size_t(dx)]=uint16_t(value>>8|value<<8);
   }
#ifdef ARDUINO_ARCH_ESP32
   if(slice.due(millis()))vTaskDelay(1);
#endif
  }
 }
#ifdef ARDUINO_ARCH_ESP32
 heap_caps_free(pixels);
#else
 std::free(pixels);
#endif
 return decoded==Result::Valid;
}
}
