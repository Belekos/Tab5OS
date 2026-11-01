#pragma once
struct StaticSemaphore_t{};using SemaphoreHandle_t=void*;
inline void* xSemaphoreCreateMutexStatic(void* p){return p;}inline void xSemaphoreTake(void*,int){}inline void xSemaphoreGive(void*){}
