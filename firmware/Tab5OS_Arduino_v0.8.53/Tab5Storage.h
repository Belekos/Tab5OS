#pragma once
#include <stdint.h>
#include <cerrno>

// Background SD users keep a lease until all their handles have closed.
// Recovery is exclusive and runs on the UI task; it never unmounts a live job.
class Tab5StorageUse {
 public:
  // Files opened through tab5FS already hold USB references. A task with only
  // such handles (or known microSD-only paths) need not block USB removal.
  explicit Tab5StorageUse(bool usbTracked = false);
  ~Tab5StorageUse();
  void release();
  bool acquire();
  explicit operator bool() const { return held_; }
  Tab5StorageUse(const Tab5StorageUse&) = delete;
  Tab5StorageUse& operator=(const Tab5StorageUse&) = delete;
 private: bool held_ = false, usbTracked_ = false;
};
class Tab5UsbStorageRecovery {
 public:
  Tab5UsbStorageRecovery();
  ~Tab5UsbStorageRecovery();
  explicit operator bool() const { return held_; }
  Tab5UsbStorageRecovery(const Tab5UsbStorageRecovery&) = delete;
  Tab5UsbStorageRecovery& operator=(const Tab5UsbStorageRecovery&) = delete;
 private: bool held_ = false;
};
class Tab5StorageRecovery {
 public:
  Tab5StorageRecovery();
  ~Tab5StorageRecovery();
  explicit operator bool() const { return held_; }
  Tab5StorageRecovery(const Tab5StorageRecovery&) = delete;
  Tab5StorageRecovery& operator=(const Tab5StorageRecovery&) = delete;
 private: bool held_ = false;
};
void tab5StorageFault(const char* operation, const char* path, int error = errno);
// Only transport failures can be repaired by remounting the SD bus.
bool tab5StorageTransportError(int error);
// Call once after the initial mount, before starting SD clients. A card that
// failed during a warm restart gets bounded background recovery as well.
void tab5StorageBootResult(bool mounted, uint32_t now);
// Manual request, called only on the UI task. Resets the bounded retry budget.
void tab5StorageRequestRecovery();
// Worker-safe: the UI owns attempt counters and bus remounting.
void tab5StorageRequestRecoveryFromWorker();
unsigned tab5StorageRecoveryCompletions();
bool tab5StorageRecoveryDue(uint32_t now);
// Set retry=false when the bus mounted but the write probe failed for a cause
// that another remount cannot repair (full/read-only card, memory exhaustion).
void tab5StorageRecoveryDone(uint32_t now, bool success, bool retry = true);
unsigned tab5StorageRecoveryAttempts();
bool tab5StorageRecoveryExhausted();
int tab5StorageLastError();
unsigned tab5StorageFailures();
unsigned tab5StorageRecoveries();
// Only the task holding the exclusive lease may probe/remount the SD bus.
bool tab5StorageExclusiveOwner();
// A deliberate eject must not trigger the automatic retry loop.
void tab5StorageEjected();
const char* tab5StorageErrorText();
void tab5StorageLog();
