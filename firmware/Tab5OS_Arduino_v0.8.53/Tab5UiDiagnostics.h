#pragma once
#include "Tab5DisplayActivity.h"
#include <stdint.h>
#include <stddef.h>

// UI task only. Disabled by default; no timer/task or filesystem IO.
// Counters allocate one PSRAM state only on first activation; no SRAM fallback.
// Durations are wall time on the UI path, including waits/preemption, NOT CPU use.
namespace Tab5UiDiagnostics {
constexpr unsigned Input=Tab5DisplayActivity::sourceCount;
constexpr unsigned Services=Input+1, Persistence=Input+2, Loop=Input+3, Count=Input+4;
struct Phase {uint64_t totalUs=0;uint32_t calls=0,maxUs=0,slow=0;};
struct Stats {Phase phases[Count];uint32_t maxGapUs=0;bool enabled=false;};
void begin();
// False only when enabling cannot obtain its bounded PSRAM state.
bool enable(bool value);
bool enabled();
Stats snapshot();
const char* phaseName(unsigned phase);
void loopBegin();
void loopEnd();
// Bounded text, constant phase names and timing numbers only. No user data.
size_t recent(char* output,size_t capacity,bool previousBoot=false);
class Scope {
 public:
  explicit Scope(unsigned phase);
  ~Scope();
  Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
 private:
  uint64_t started_=0;uint32_t generation_=0;unsigned phase_;bool active_=false;
};
class Activity {
 public:
  explicit Activity(Tab5DisplayActivity::Source source):display_(source),timing_(unsigned(source)){}
 private:
  Tab5DisplayActivity::Scope display_;Scope timing_;
};
}
