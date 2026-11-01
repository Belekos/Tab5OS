#pragma once
#include <cstdint>
using TickType_t=uint32_t;
constexpr int pdTRUE=1,pdFALSE=0;
#define pdMS_TO_TICKS(ms) (ms)
#define portTICK_PERIOD_MS 1
