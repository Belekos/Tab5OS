#pragma once
#include "Tab5ResourceManager.h"
#include <algorithm>
#include <string>
#include <vector>

namespace Tab5Git {
// The worker stack and this working set must coexist. Reserve the working set
// before creating the worker, so the OS reclaims for their combined demand.
// The worker takes ownership after the task's stack admission is consumed.
inline bool needsNetwork(const std::vector<std::string>& argv) {
  const size_t n=argv.size()>1&&argv[1]=="-C"?3:1;
  if(n>=argv.size())return false;
  const auto& operation=argv[n];
  return operation=="clone"||operation=="fetch"||operation=="pull"||operation=="push";
}
inline Tab5Resources::Budget startupBudget(bool networkOperation) {
  auto budget=networkOperation?Tab5Resources::tlsBudget():Tab5Resources::Budget{};
  budget.internal+=8*1024;budget.psram+=128*1024;
  budget.psramBlock=std::max(budget.psramBlock,size_t(128*1024));
  return budget;
}
inline Tab5Resources::Reservation reserveStartup(
    const std::vector<std::string>& argv,Tab5Resources::AdmissionReport* report) {
  return tab5ReserveMemory(Tab5Resources::Owner::Tools,
      startupBudget(needsNetwork(argv)),"git-session",report);
}

// Reserve only the next allocation batch. Allocated bytes are already visible
// in the live heap sample; freed bytes return to that sample, not to an old
// batch. A session uses at most one batch slot plus its TLS/startup slot.
class AllocationBudget {
 public:
  static constexpr size_t BatchBytes = 256u * 1024u;
  bool admit(size_t bytes) {
    if (bytes <= pending) return true;
    reserve.release();
    pending = 0;
    const size_t batch = std::max(bytes, BatchBytes);
    reserve = tab5ReserveMemory(Tab5Resources::Owner::Tools,
        {0, 0, 0, batch, bytes}, "git-psram", &report);
    size_t admitted = batch;
    if (!reserve && bytes < batch &&
        report.reason == Tab5Resources::Denial::Psram) {
      // Cache collection has already run. A small allocation may still fit
      // even when a complete batch does not; keep the same system margins.
      reserve = Tab5MemoryManager.reserve(Tab5Resources::Owner::Tools,
          {0, 0, 0, bytes, bytes}, &report);
      admitted = bytes;
    }
    if (!reserve) return false;
    pending = admitted;
    return true;
  }
  void allocated(size_t bytes) {
    reserve.allocated(0, bytes);
    pending -= std::min(pending, bytes);
  }
  const Tab5Resources::AdmissionReport& lastReport() const { return report; }
 private:
  Tab5Resources::Reservation reserve;
  Tab5Resources::AdmissionReport report{};
  size_t pending = 0;
};
} // namespace Tab5Git
