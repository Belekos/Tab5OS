#pragma once
#include <stdint.h>
#include <stddef.h>

// Browser preferences, never a playback request. Only the UI thread accesses
// State; volume() may be called by a worker and never performs flash I/O.
namespace Tab5MediaSession {
struct State {
  uint8_t view = 0;
  uint8_t favorites = 0;
  uint8_t reserved[2]{};
  int32_t scroll[4]{};
  char folder[272]{};
  char category[192]{};
  char selected[4][512]{};
  char title[4][128]{};
  char search[4][96]{};
};
bool begin(uint8_t legacyVolume, bool legacyAvailable = true);
// A delayed SD migration may make old preferences available only after boot.
// Never replaces an existing NVS value or a volume chosen since this boot.
bool seedLegacyVolume(uint8_t value, uint32_t now);
const State& state();
uint8_t volume();
void volume(uint8_t value, uint32_t now);
void browser(uint8_t view, uint8_t favorites, const char* folder,
             const char* category, uint32_t now);
void select(uint8_t view, const char* identity, const char* title, uint32_t now);
void search(uint8_t view, const char* query, uint32_t now);
void scroll(uint8_t view, int32_t y, uint32_t now);
// Coalesces changes for 1200 ms. Unchanged states cause no writes. Failed
// writes stay pending and are retried at most once every five seconds.
void service(uint32_t now);
// Requests an immediate asynchronous commit. True only once all requested
// changes are durable; false while saving or after an error. UI must not wait.
bool flush(uint32_t now);
bool saving();
struct Diagnostics {
  uint32_t started = 0, completed = 0, failed = 0, deferred = 0;
  uint32_t lastWriteUs = 0, maxWriteUs = 0;
  uint64_t lastBeginUs = 0, lastEndUs = 0;
  uint32_t underrunsDuringWrites = 0;
  bool dsiObserved = false;
};
Diagnostics snapshot(); // UI thread; no I/O and no periodic logging.
}
