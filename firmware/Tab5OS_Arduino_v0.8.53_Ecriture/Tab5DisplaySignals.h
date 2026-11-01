#pragma once
#include <stdint.h>

struct Tab5DisplaySignalsStats {
  uint64_t lastUnderrunUs = 0;
  uint32_t interrupts = 0;
  uint32_t underruns = 0;
  uint32_t lastStatus = 0;
  uint32_t allocatedFlags = 0;
  bool active = false;
};

// Passive snapshot. A false active flag means no instrumented DSI interrupt
// was installed; zero underruns must not then be interpreted as a measurement.
Tab5DisplaySignalsStats tab5DisplaySignalsStats();

// One consumer, in task context only. The first event is immediate; subsequent
// events are coalesced to at most one report per second, with no count loss.
bool tab5DisplayTakeUnderrunEvent(uint32_t nowMs, uint32_t* delta);
