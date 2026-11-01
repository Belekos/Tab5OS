#pragma once
#include <assert.h>
#include <Arduino.h>
struct StaticSemaphore_t { bool held = false; };
using SemaphoreHandle_t = StaticSemaphore_t*;
extern unsigned testTakes, testGives;
extern void (*testTakeHook)();
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* state) { return state; }
inline int xSemaphoreTake(SemaphoreHandle_t gate, unsigned wait) {
  ++testTakes;
  if (testTakeHook) testTakeHook();
  if (gate->held) { testMilliseconds += wait; return pdFALSE; }
  gate->held = true;
  return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t gate) {
  assert(gate->held);
  ++testGives;
  gate->held = false;
}
