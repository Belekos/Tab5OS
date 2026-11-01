#pragma once
struct StaticSemaphore_t{};using SemaphoreHandle_t=StaticSemaphore_t*;constexpr unsigned portMAX_DELAY=0;
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* p){return p;}inline void xSemaphoreTake(SemaphoreHandle_t,unsigned){}inline void xSemaphoreGive(SemaphoreHandle_t){}
