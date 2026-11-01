#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
// M5GFX reuses one static PNG decoder across every canvas and task.
class Tab5ImageLock {
  bool held_;
  static SemaphoreHandle_t mutex() {
    static StaticSemaphore_t storage;
    static SemaphoreHandle_t handle=xSemaphoreCreateMutexStatic(&storage);
    return handle;
  }
public:
  explicit Tab5ImageLock(unsigned waitMs=25):held_(xSemaphoreTake(mutex(),pdMS_TO_TICKS(waitMs))==pdTRUE){}
  ~Tab5ImageLock(){if(held_)xSemaphoreGive(mutex());}
  explicit operator bool()const{return held_;}
  Tab5ImageLock(const Tab5ImageLock&)=delete;
};
