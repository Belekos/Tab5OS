#pragma once
#include <stdint.h>

// UI-thread start/takeCompletion; cancel and snapshot are safe while the job runs.
// No LVGL calls in the worker. Destination must be a new full path, not a folder
// to merge into. move marks the requested intent, but this fallback retains the
// source: concurrent writers make copy-then-delete unsafe without a shared lock.
// A failed/cancelled copy may leave a partial destination: never
// recursively delete it without an explicit user request.
namespace Tab5FileOperation {
constexpr unsigned PathCapacity = 256;
enum class Phase : uint8_t { Idle, Scanning, Copying, Verifying, Removing, Done, Failed, Cancelled };
struct Snapshot {
  Phase phase = Phase::Idle;
  uint32_t filesDone = 0, filesTotal = 0;
  uint64_t bytesDone = 0, bytesTotal = 0;
  int error = 0;
};
struct Completion {
  bool success = false;          // Destination is a complete, verified copy.
  bool cancelled = false;
  bool moved = false;            // Always false for this non-destructive fallback.
  bool sourceRetained = true;    // No source is removed by this worker.
  bool destinationPresent = false;
  bool sourceIsDirectory = false;
  bool requestedMove = false;
  int error = 0;
  char source[PathCapacity]{};
  char destination[PathCapacity]{};
};
bool startCopy(const char* source, const char* destination, bool move);
bool busy();
void cancel();
void snapshot(Snapshot& out);
const char* status();
bool takeCompletion(Completion& out); // Acknowledge once before another start.
} // namespace Tab5FileOperation
