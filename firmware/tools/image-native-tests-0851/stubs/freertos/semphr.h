#pragma once
#include "FreeRTOS.h"
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* m){return m;}
inline int xSemaphoreTake(SemaphoreHandle_t m,unsigned){m->lock();return pdTRUE;}
inline void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
