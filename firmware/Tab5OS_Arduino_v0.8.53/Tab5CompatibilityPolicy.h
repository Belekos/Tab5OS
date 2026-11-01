#pragma once

#include <stdint.h>

namespace Tab5Compatibility {

struct Version {
  uint32_t major = 0;
  uint32_t minor = 0;
  uint32_t patch = 0;
};

enum class Firmware : uint8_t {
  NotChecked, Unknown, SameVersion, DifferentVersion
};
enum class Transport : uint8_t { NotStarted, Up, Down, Failed };
enum class Display : uint8_t { Unknown, Ili9881cGt911, St7121, St7123 };
enum class Event : uint8_t { CoprocessorInit, Up, Down, Failure };

struct Snapshot {
  Version host;
  Version coprocessor;
  Firmware firmware = Firmware::NotChecked;
  Transport transport = Transport::NotStarted;
  Display display = Display::Unknown;
  int32_t queryError = 0;
  uint32_t coprocessorResetReason = 0;
  uint32_t transportFailures = 0;
  uint16_t chipRevision = 0;
  uint16_t minimumChipRevision = 0;
  uint16_t maximumChipRevision = 0;
  bool chipRevisionRangeKnown = false;
  bool eventMonitoring = false;
};

// ESP-Hosted packs each release component into eight bits.
inline bool validVersion(Version version) {
  return version.major > 0 && version.major <= 255 &&
         version.minor <= 255 && version.patch <= 255;
}

inline Firmware compareVersions(Version host, Version remote, bool obtained) {
  if (!obtained || !validVersion(host) || !validVersion(remote))
    return Firmware::Unknown;
  if (host.major == remote.major && host.minor == remote.minor &&
      host.patch == remote.patch)
    return Firmware::SameVersion;
  // Version numbers alone do not establish protocol compatibility. This is
  // diagnostic information only, including a different major version.
  return Firmware::DifferentVersion;
}

inline Display identifyDisplay(bool tab5, int touchAddress,
                               bool stReadSucceeded, uint8_t stVersion) {
  if (!tab5) return Display::Unknown;
  if (touchAddress == 0x14) return Display::Ili9881cGt911;
  if (touchAddress != 0x55 || !stReadSucceeded) return Display::Unknown;
  if (stVersion == 1) return Display::St7121;
  if (stVersion == 3) return Display::St7123;
  return Display::Unknown;
}

// No I/O, allocation, or locking here. The adapter below serializes these
// transitions with its short critical section; no RPC is made under that lock.
class State {
 public:
  explicit State(Version host) { _snapshot.host = host; }
  Snapshot snapshot() const { return _snapshot; }
  void setDisplay(Display value) { _snapshot.display = value; }
  void setEventMonitoring(bool value) { _snapshot.eventMonitoring = value; }
  void setChipRevision(uint16_t actual, uint16_t minimum, uint16_t maximum) {
    _snapshot.chipRevision = actual;
    _snapshot.minimumChipRevision = minimum;
    _snapshot.maximumChipRevision = maximum;
    _snapshot.chipRevisionRangeKnown = true;
  }
  void event(Event event, uint32_t resetReason = 0) {
    if (event == Event::Up) {
      _snapshot.transport = Transport::Up;
      return;
    }
    ++_generation;
    _attempted = false;
    _snapshot.coprocessor = {};
    _snapshot.queryError = 0;
    _snapshot.firmware = Firmware::NotChecked;
    if (event == Event::CoprocessorInit) {
      _snapshot.coprocessorResetReason = resetReason;
      _snapshot.transport = Transport::NotStarted;
    } else if (event == Event::Failure) {
      _snapshot.transport = Transport::Failed;
      if (_snapshot.transportFailures != UINT32_MAX) ++_snapshot.transportFailures;
    } else {
      _snapshot.transport = Transport::Down;
    }
  }
  bool beginProbe(uint32_t& generation) {
    if (_attempted || _inFlight) return false;
    _attempted = true;
    _inFlight = true;
    generation = _generation;
    return true;
  }
  bool finishProbe(uint32_t generation, Version remote, int32_t error) {
    _inFlight = false;
    if (generation != _generation) return false;
    _snapshot.queryError = error;
    _snapshot.firmware = compareVersions(_snapshot.host, remote, error == 0);
    if (_snapshot.firmware != Firmware::Unknown) _snapshot.coprocessor = remote;
    return true;
  }
 private:
  Snapshot _snapshot;
  uint32_t _generation = 0;
  bool _attempted = false;
  bool _inFlight = false;
};

}  // namespace Tab5Compatibility
