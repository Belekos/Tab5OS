#pragma once
#include "FreeRTOS.h"
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(){return new std::recursive_mutex;}
inline void xSemaphoreTakeRecursive(SemaphoreHandle_t m,int){m->lock();}
inline void xSemaphoreGiveRecursive(SemaphoreHandle_t m){m->unlock();}
inline void vSemaphoreDelete(SemaphoreHandle_t m){delete m;}
