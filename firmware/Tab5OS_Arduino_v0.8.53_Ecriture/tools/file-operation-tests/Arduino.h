#pragma once
#include <stdint.h>
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
void testTick();
inline void vTaskDelay(unsigned) { testTick(); }
inline void vTaskDelete(void*) {}
