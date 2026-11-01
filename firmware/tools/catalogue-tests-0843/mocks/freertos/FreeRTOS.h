#pragma once
#include <cstdint>
using BaseType_t=int;
using TickType_t=uint32_t;
using TaskFunction_t=void(*)(void*);
using TaskHandle_t=void*;
constexpr BaseType_t pdPASS=1,pdFAIL=0;
#define pdMS_TO_TICKS(ms) (TickType_t(ms))
