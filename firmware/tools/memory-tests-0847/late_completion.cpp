#include "Tab5ResourceManager.h"
#include <freertos/event_groups.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>

using namespace Tab5Resources;
std::atomic<unsigned> delays{0}, takes{0}, gives{0}, mutexCreates{0};
std::atomic<unsigned> eventWaits{0}, eventSets{0}, eventCreates{0};
std::function<void()> beforeEventWait, beforeEventSet;
bool tab5TlsMemoryExternal() { return true; }
static std::atomic<size_t> available{30 * KiB};
static unsigned checks = 0;
static void check(bool result, const char* reason) {
  ++checks;
  if (!result) { std::fprintf(stderr, "FAIL: %s\n", reason); std::abort(); }
}
template<class Predicate> static void until(Predicate ready) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!ready()) {
    if (std::chrono::steady_clock::now() >= end) check(false, "test barrier completed");
    std::this_thread::yield();
  }
}
static MemorySample sample() {
  const size_t n = available.load();
  return {n, n, n, 16 * MiB, 16 * MiB};
}
static MemoryTotals totals() { return {available.load(), 16 * MiB}; }
static MemorySample requested(const Budget&) { return sample(); }
static size_t reclaim(void* context) {
  ++*static_cast<std::atomic<unsigned>*>(context);
  available = 200 * KiB;
  return 170 * KiB;
}

int main() {
  constexpr unsigned cycles = 8;
  constexpr Budget stack{16 * KiB, 16 * KiB, 0, 0, 0};
  for (unsigned cycle = 0; cycle < cycles; ++cycle) {
    available = 30 * KiB;
    eventWaits = eventSets = 0;
    Manager manager(sample, totals, requested);
    manager.begin();
    std::atomic<unsigned> callbacks{0}, setEntries{0};
    manager.addCache(Owner::Files, "reloadable", reclaim, &callbacks);
    manager.foreground(Owner::Audio);
    std::atomic<bool> oldSetterPaused{false}, releaseOldSetter{false};
    beforeEventSet = [&] {
      if (setEntries.fetch_add(1) == 0) {
        oldSetterPaused = true;
        until([&] { return releaseOldSetter.load(); });
      }
    };
    bool acceptedFirst = false, acceptedSecond = false;
    std::atomic<bool> doneSecond{false};
    std::thread first([&] {
      auto reservation = manager.reserveRecovering(Owner::Audio, stack, false, nullptr, 100);
      acceptedFirst = bool(reservation);
    });
    until([&] { return eventWaits.load() == 1; });
    std::thread collector([&] { manager.serviceCollections(); });
    until([&] { return oldSetterPaused.load(); });
    // The request is Done, but its native SetBits has not run. The wait expires,
    // the caller frees the request slot, then its final admission succeeds.
    first.join();
    check(acceptedFirst && callbacks == 1, "first Done request is retired before old SetBits");
    available = 30 * KiB;
    std::thread second([&] {
      auto reservation = manager.reserveRecovering(Owner::Audio, stack, false, nullptr, 1000);
      acceptedSecond = bool(reservation);
      doneSecond = true;
    });
    until([&] { return eventWaits.load() == 2; });
    releaseOldSetter = true;
    collector.join();
    // The old notification wakes the new request once. Its Pending state must
    // force another wait instead of admitting a workload before collection.
    until([&] { return eventWaits.load() >= 3; });
    check(!doneSecond && callbacks == 1, "late old event cannot complete a reused Pending slot");
    manager.serviceCollections();
    second.join();
    check(acceptedSecond && callbacks == 2, "new request completes only after its own collection");
    check(manager.denied() == 0 && eventSets == 2, "no false refusal or duplicate collector completion");
    beforeEventSet = {};
  }
  std::printf("{\"passed\":true,\"checks\":%u,\"lateCompletionReuseCycles\":%u}\n", checks, cycles);
}
