#pragma once
#include "FreeRTOS.h"
#include <atomic>
#include <chrono>
#include <thread>
extern std::atomic<unsigned> delays;
inline TickType_t xTaskGetTickCount(){return TickType_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
inline void vTaskDelay(TickType_t t){++delays;std::this_thread::sleep_for(std::chrono::milliseconds(t));}
