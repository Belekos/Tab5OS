#pragma once
#include "FreeRTOS.h"
inline SemaphoreHandle_t xSemaphoreCreateMutex() { static int mutex; return &mutex; }
inline int xSemaphoreTake(SemaphoreHandle_t handle, uint32_t) { return handle ? pdTRUE : 0; }
inline void xSemaphoreGive(SemaphoreHandle_t) {}
