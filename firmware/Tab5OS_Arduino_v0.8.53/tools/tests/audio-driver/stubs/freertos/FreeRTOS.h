#pragma once
#include <cstdint>
using BaseType_t = int;
using SemaphoreHandle_t = void*;
constexpr int pdTRUE = 1;
constexpr uint32_t portMAX_DELAY = UINT32_MAX;
#define pdMS_TO_TICKS(value) (value)
inline void vTaskDelay(uint32_t) {}
