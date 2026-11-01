#include <assert.h>
#include <stdio.h>
#include <Arduino.h>
#include "Tab5NetworkGate.h"

uint32_t testMilliseconds = 0;
unsigned testTakes = 0, testGives = 0;
void (*testDelayHook)() = nullptr;
void (*testTakeHook)() = nullptr;
TestSerial Serial;

static void disable() { tab5NetworkSetEnabled(false); }
static bool cancelled(void*) { return true; }

int main() {
  { Tab5NetworkLease lease("uninitialized", 100); assert(!lease); }
  tab5NetworkGateBegin();

  tab5NetworkSetEnabled(false);
  { Tab5NetworkLease lease("disabled", 100); assert(!lease); assert(testTakes == 0); }
  assert(!tab5NetworkBusy());
  assert(!tab5NetworkBackgroundYieldRequested());

  tab5NetworkSetEnabled(true);
  {
    Tab5NetworkLease first("first", 100);
    assert(first && tab5NetworkBusy());
    testTakeHook = disable;
    Tab5NetworkLease waiting("waiting", 100);
    testTakeHook = nullptr;
    assert(!waiting && testMilliseconds == 20);
    // Rejecting a waiter must not release the active task's mutex.
    assert(tab5NetworkBusy() && testGives == 0);
  }
  assert(!tab5NetworkBusy() && testGives == 1);
  assert(!tab5NetworkBackgroundYieldRequested());

  tab5NetworkSetEnabled(true);
  testTakeHook = disable;
  { Tab5NetworkLease lease("disabled-during-acquisition", 100); assert(!lease); }
  testTakeHook = nullptr;
  assert(!tab5NetworkBusy() && testGives == 2);

  tab5NetworkSetEnabled(true);
  tab5NetworkSetBackgroundRetained(true);
  testDelayHook = disable;
  unsigned before = testTakes;
  { Tab5NetworkLease lease("retained-background", 100); assert(!lease); }
  testDelayHook = nullptr;
  tab5NetworkSetBackgroundRetained(false);
  assert(testTakes == before && !tab5NetworkBackgroundYieldRequested());

  tab5NetworkSetEnabled(true);
  { Tab5NetworkLease lease("cancelled", 100, cancelled); assert(!lease); }
  assert(testTakes == before && !tab5NetworkBackgroundYieldRequested());
  {
    Tab5NetworkLease lease("re-enabled", 100);
    assert(lease && tab5NetworkBusy());
  }
  assert(!tab5NetworkBusy() && !tab5NetworkBackgroundYieldRequested());
  puts("PASS: disabled admission, waiting cancellation, acquisition race, mutex ownership, re-enable");
}
