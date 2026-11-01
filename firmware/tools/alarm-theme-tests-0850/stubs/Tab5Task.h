#pragma once
#include "Arduino.h"
#define pdPASS 1
#define pdFAIL 0
#define pdMS_TO_TICKS(n) (n)
extern bool taskCreateOk;
extern void(*pendingTask)(void*);
extern void* pendingArgument;
inline int tab5CreateTask(void(*f)(void*),const char*,unsigned,void* arg,unsigned,void*,int){if(!taskCreateOk)return pdFAIL;pendingTask=f;pendingArgument=arg;return pdPASS;}
inline void vTaskDelay(unsigned n){testMillis+=n;}
inline void vTaskDelete(void*){}
inline void tab5TaskCheckpoint(const char*){}
