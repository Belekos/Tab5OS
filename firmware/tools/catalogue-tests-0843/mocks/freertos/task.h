#pragma once
#include "FreeRTOS.h"
#include <chrono>
#include <thread>
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t,const char*,uint32_t,void*,unsigned,TaskHandle_t*,int);
inline TickType_t xTaskGetTickCount(){return TickType_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
inline void vTaskDelay(unsigned ticks){std::this_thread::sleep_for(std::chrono::milliseconds(ticks));}
