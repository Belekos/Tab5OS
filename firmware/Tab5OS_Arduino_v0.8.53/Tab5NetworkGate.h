#pragma once
#include <stdint.h>

// Serializes network operations and their connection setup. The single radio
// may retain an established stream alongside one serialized operation, after
// its memory admission and setup complete. Driver transitions wait for both.
// Lease destruction runs on the opening task (FreeRTOS mutex ownership).
// Admission checks the OS network switch both before and after waiting.
// Existing clients must still poll tab5NetworkEnabled() and close on disable.
// This cooperative gate does not route packets, enforce a VPN, or sandbox apps.
void tab5NetworkGateBegin();
bool tab5NetworkEnabled();
void tab5NetworkSetEnabled(bool enabled);
bool tab5NetworkBusy();
bool tab5NetworkMediaPending();
bool tab5NetworkBackgroundYieldRequested();
void tab5NetworkSetMediaPending(bool pending);
// Map worker acknowledges that retained sockets/TLS buffers have closed.
void tab5NetworkSetBackgroundRetained(bool retained);
using Tab5NetworkCancel = bool (*)(void*);
class Tab5NetworkLease {
 public:
  Tab5NetworkLease(const char* owner, uint32_t timeoutMs,
                  Tab5NetworkCancel cancelled = nullptr, void* context = nullptr,
                  bool background = false);
  ~Tab5NetworkLease();
  explicit operator bool() const { return _locked || _shared; }
  Tab5NetworkLease(const Tab5NetworkLease&) = delete;
  Tab5NetworkLease& operator=(const Tab5NetworkLease&) = delete;
 private:
  friend class Tab5NetworkTransition;
  friend class Tab5AudioEngine;
  void shareAfterSetup();
  enum class Admission { Client, OsTransition };
  Tab5NetworkLease(const char* owner, uint32_t timeoutMs,
                   Tab5NetworkCancel cancelled, void* context,
                   bool background, Admission admission);
  bool _locked = false;
  bool _shared = false;
  bool _foreground = false;
};

// Only the OS radio owners may change the driver while client admission is off.
// Never exposed through the application or MicroPython APIs. The transition
// still waits for existing owners and retained background sockets to release.
class Tab5NetworkTransition {
 public:
  explicit operator bool() const { return static_cast<bool>(_lease); }
  Tab5NetworkTransition(const Tab5NetworkTransition&) = delete;
  Tab5NetworkTransition& operator=(const Tab5NetworkTransition&) = delete;
 private:
  friend class Tab5AudioEngine;
  friend class Tab5FileShareNetworkTransition;
  Tab5NetworkTransition(const char* owner, uint32_t timeoutMs);
  Tab5NetworkLease _lease;
};
