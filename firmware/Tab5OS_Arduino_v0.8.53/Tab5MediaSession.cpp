#include "Tab5MediaSession.h"
#include "Tab5Task.h"
#include "Tab5DisplaySignals.h"
#include <Preferences.h>
#include <esp_timer.h>
#include <atomic>
#include <cstring>
#include <new>
#include <algorithm>

namespace Tab5MediaSession {
namespace {
constexpr uint32_t Magic = 0x4D353254, Version = 1;
constexpr uint32_t Debounce = 1200, Retry = 5000;
struct Record { uint32_t magic, version, size, checksum; State state; };
State* current = nullptr;
const State empty{};
std::atomic<uint8_t> wantedVolume{150};
std::atomic<uint32_t> volumeChanged{0};
std::atomic<bool> volumeTouched{false};
uint8_t savedVolume = 150;
uint32_t savedHash = 0, changedAt = 0, failedAt = 0;
bool dirty = false, failed = false, started = false, knownVolume = false;
bool legacyReady = false;
struct CommitJob {
  Record record{};
  uint8_t volume = 0;
  bool saveVolume = false, saveState = false;
  bool volumeOk = false, stateOk = false;
  uint32_t durationUs = 0;
  uint64_t beganUs = 0, endedUs = 0;
  uint32_t underruns = 0;
  bool dsiObserved = false;
  std::atomic<bool> done{false};
};
CommitJob* pending = nullptr; // UI owns the pointer; worker owns its immutable snapshot.
bool forceRequested = false;
Diagnostics diagnostics{};

void writeJob(void* argument) {
  auto* job = static_cast<CommitJob*>(argument);
  const int64_t began = esp_timer_get_time();
  const auto before = tab5DisplaySignalsStats();
  Preferences prefs;
  if (prefs.begin("tab5_media", false)) {
    if (job->saveVolume)
      job->volumeOk = prefs.putBytes("volume", &job->volume, sizeof(job->volume)) == sizeof(job->volume);
    if (job->saveState)
      job->stateOk = prefs.putBytes("browser", &job->record, sizeof(job->record)) == sizeof(job->record);
    prefs.end();
  }
  job->beganUs = uint64_t(began); job->endedUs = uint64_t(esp_timer_get_time());
  job->durationUs = uint32_t(std::min<uint64_t>(UINT32_MAX, job->endedUs - job->beganUs));
  const auto after = tab5DisplaySignalsStats();
  job->dsiObserved = before.active && after.active;
  if (job->dsiObserved) job->underruns = after.underruns - before.underruns;
  // No access to the snapshot after publishing completion: the UI may free it.
  job->done.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

void finish(uint32_t now) {
  if (!pending || !pending->done.load(std::memory_order_acquire)) return;
  auto* job = pending;
  const bool ok = (!job->saveVolume || job->volumeOk) && (!job->saveState || job->stateOk);
  if (job->saveVolume && job->volumeOk) { savedVolume = job->volume; knownVolume = true; }
  if (job->saveState && job->stateOk) savedHash = job->record.checksum;
  diagnostics.maxWriteUs = std::max(diagnostics.maxWriteUs, job->durationUs);
  diagnostics.lastWriteUs = job->durationUs;
  diagnostics.lastBeginUs = job->beganUs; diagnostics.lastEndUs = job->endedUs;
  diagnostics.dsiObserved = job->dsiObserved;
  diagnostics.underrunsDuringWrites += job->underruns;
  ++diagnostics.completed;
  if (!ok) ++diagnostics.failed;
  job->~CommitJob(); heap_caps_free(job); pending = nullptr;
  failed = !ok;
  if (failed) failedAt = now;
}
uint32_t hash(const State& value) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
  uint32_t h = 2166136261U;
  for (size_t i = 0; i < sizeof(value); ++i) h = (h ^ bytes[i]) * 16777619U;
  return h;
}
template<size_t N> bool terminated(const char (&text)[N]) {
  return std::memchr(text, 0, N) != nullptr;
}
bool valid(const State& s) {
  if (s.view > 3 || s.favorites > 3 || s.reserved[0] || s.reserved[1] ||
      !terminated(s.folder) || !terminated(s.category)) return false;
  for (unsigned i = 0; i < 4; ++i)
    if (!terminated(s.selected[i]) || !terminated(s.title[i]) ||
        !terminated(s.search[i]) || s.scroll[i] < 0 || s.scroll[i] > 1000000)
      return false;
  return true;
}
template<size_t N> bool text(char (&destination)[N], const char* source) {
  if (!source) source = "";
  // Never store a truncated path/URL which could identify another item.
  size_t size = 0;
  while (size < N && source[size]) ++size;
  if (size == N) { source = ""; size = 0; }
  if (!std::strcmp(destination, source)) return false;
  std::memset(destination, 0, N);
  std::memcpy(destination, source, size);
  return true;
}
void changed(bool value, uint32_t now) {
  if (value) { dirty = true; changedAt = now; }
}
bool commit(uint32_t now, bool force) {
  if (!started) return true;
  finish(now);
  forceRequested = forceRequested || force;
  if (pending) return false;
  const uint8_t desired = wantedVolume.load();
  const bool saveVolume = ((!knownVolume && (legacyReady || volumeTouched.load())) || desired != savedVolume) &&
      (forceRequested || uint32_t(now - volumeChanged.load()) >= Debounce);
  bool saveState = dirty && current &&
      (forceRequested || uint32_t(now - changedAt) >= Debounce);
  uint32_t h = savedHash;
  if (saveState) {
    h = hash(*current);
    if (h == savedHash) { dirty = false; saveState = false; }
  }
  if (!saveVolume && !saveState) { forceRequested = false; return true; }
  if (failed && uint32_t(now - failedAt) < Retry) return false;
  constexpr uint32_t StackBytes = 6 * 1024;
  auto budget = Tab5Resources::TaskBudget(StackBytes);
  budget.psram = budget.psramBlock = sizeof(CommitJob);
  // No cache recovery or wait for persistence completion in the UI.
  auto admission = Tab5MemoryManager.reserve(Tab5Resources::Owner::System, budget);
  if (admission) {
    void* memory = heap_caps_malloc(sizeof(CommitJob), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (memory) {
      auto* job = new (memory) CommitJob;
      admission.allocated(0, sizeof(CommitJob));
      job->volume = desired; job->saveVolume = saveVolume; job->saveState = saveState;
      if (saveState) {
        job->record.magic = Magic; job->record.version = Version;
        job->record.size = sizeof(State); job->record.checksum = h; job->record.state = *current;
      }
      if (tab5CreateAdmittedTask(admission, writeJob, "media_prefs", StackBytes,
          job, 1, nullptr, tskNO_AFFINITY) == pdPASS) {
        pending = job; forceRequested = false; ++diagnostics.started;
        return false;
      }
      job->~CommitJob(); heap_caps_free(job);
    }
  }
  ++diagnostics.deferred;
  failed = true; failedAt = now;
  return false;
}
}
bool begin(uint8_t legacyVolume, bool legacyAvailable) {
  // Boot-only in production; do not invalidate a snapshot still owned by a worker.
  if (pending) return false;
  delete current;
  current = new (std::nothrow) State;
  savedVolume = legacyVolume; knownVolume = false; legacyReady = legacyAvailable;
  Preferences prefs;
  if (prefs.begin("tab5_media", true)) {
    if (prefs.getBytesLength("volume") == sizeof(savedVolume))
      knownVolume = prefs.getBytes("volume", &savedVolume, sizeof(savedVolume)) == sizeof(savedVolume);
    if (current && prefs.getBytesLength("browser") == sizeof(Record)) {
      Record* record = new (std::nothrow) Record;
      if (record) {
        if (prefs.getBytes("browser", record, sizeof(*record)) == sizeof(*record) &&
            record->magic == Magic && record->version == Version &&
            record->size == sizeof(State) && valid(record->state) &&
            record->checksum == hash(record->state)) *current = record->state;
        delete record;
      }
    }
    prefs.end();
  }
  wantedVolume = savedVolume; volumeChanged = 0; volumeTouched = false;
  savedHash = hash(state()); dirty = failed = false;
  changedAt = failedAt = 0; started = true;
  forceRequested = false;
  return current != nullptr;
}
bool seedLegacyVolume(uint8_t value,uint32_t now) {
  if(knownVolume||volumeTouched.load())return false;
  wantedVolume=value;savedVolume=value;volumeChanged=now;legacyReady=true;
  return true;
}
const State& state() { return current ? *current : empty; }
uint8_t volume() { return wantedVolume.load(); }
void volume(uint8_t value, uint32_t now) {
  const bool firstChoice=!volumeTouched.exchange(true);
  if (wantedVolume.exchange(value) != value || firstChoice) volumeChanged.store(now);
}
void browser(uint8_t view, uint8_t favorites, const char* folder,
             const char* category, uint32_t now) {
  if (!current || view > 3) return;
  favorites &= 3;
  bool different = current->view != view || current->favorites != favorites;
  current->view = view; current->favorites = favorites;
  different = text(current->folder, folder) || different;
  different = text(current->category, category) || different;
  changed(different, now);
}
void select(uint8_t view, const char* identity, const char* title, uint32_t now) {
  if (!current || view > 3) return;
  bool different = text(current->selected[view], identity);
  different = text(current->title[view], title) || different;
  changed(different, now);
}
void search(uint8_t view, const char* query, uint32_t now) {
  if (current && view < 4) changed(text(current->search[view], query), now);
}
void scroll(uint8_t view, int32_t y, uint32_t now) {
  if (!current || view > 3) return;
  y = std::max<int32_t>(0, std::min<int32_t>(1000000, y));
  const bool different = current->scroll[view] != y;
  current->scroll[view] = y; changed(different, now);
}
void service(uint32_t now) { commit(now, false); }
bool flush(uint32_t now) { return commit(now, true); }
bool saving() { return pending != nullptr; }
Diagnostics snapshot() { return diagnostics; }
}
