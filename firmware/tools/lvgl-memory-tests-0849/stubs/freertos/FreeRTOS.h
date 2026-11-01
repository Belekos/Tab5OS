#pragma once
#include <mutex>
using StaticSemaphore_t=std::mutex; using SemaphoreHandle_t=std::mutex*;
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffU
