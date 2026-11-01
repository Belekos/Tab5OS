#include "Tab5Serial.h"
#include "Tab5NetworkGate.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include <atomic>
#include <algorithm>

namespace {
StaticSemaphore_t gateStorage;
SemaphoreHandle_t gate = nullptr;
StaticEventGroup_t idleStorage;
EventGroupHandle_t idle = nullptr;
constexpr EventBits_t BackgroundClosed = 1U, RadioClosed = 2U;
std::atomic<bool> enabled{true}, exclusiveBusy{false}, mediaPending{false};
std::atomic<unsigned> foregroundSessions{0}, sharedSessions{0};
std::atomic<bool> backgroundRetained{false};
}

bool tab5NetworkEnabled() { return enabled.load(); }
void tab5NetworkSetEnabled(bool value) { enabled = value; }
bool tab5NetworkBusy() { return exclusiveBusy.load() || sharedSessions.load() != 0; }
bool tab5NetworkMediaPending() { return mediaPending.load(); }
bool tab5NetworkBackgroundYieldRequested() { return mediaPending.load() || foregroundSessions.load() != 0; }
void tab5NetworkSetBackgroundRetained(bool retained) {
  backgroundRetained = retained;
  if (idle) {
    if (retained) xEventGroupClearBits(idle, BackgroundClosed);
    else xEventGroupSetBits(idle, BackgroundClosed);
  }
}
void tab5NetworkSetMediaPending(bool value) { mediaPending = value; }

void tab5NetworkGateBegin() {
  // Called once during boot, before any network worker is admitted.
  if (!gate) gate = xSemaphoreCreateMutexStatic(&gateStorage);
  if (!idle) {
    idle = xEventGroupCreateStatic(&idleStorage);
    xEventGroupSetBits(idle, BackgroundClosed | RadioClosed);
  }
}

Tab5NetworkLease::Tab5NetworkLease(const char* owner, uint32_t timeoutMs,
                                 Tab5NetworkCancel cancelled, void* context, bool background)
    : Tab5NetworkLease(owner, timeoutMs, cancelled, context, background, Admission::Client) {}

Tab5NetworkTransition::Tab5NetworkTransition(const char* owner, uint32_t timeoutMs)
    : _lease(owner, timeoutMs, nullptr, nullptr, false, Tab5NetworkLease::Admission::OsTransition) {}

Tab5NetworkLease::Tab5NetworkLease(const char* owner, uint32_t timeoutMs,
                                 Tab5NetworkCancel cancelled, void* context, bool background,
                                 Admission admission) {
  const bool control = admission == Admission::OsTransition;
  if (!gate || !idle || (!control && !enabled.load())) return;
  _foreground = !background;
  if (_foreground) ++foregroundSessions;
  const uint32_t started = millis();
  do {
    if ((!control && !enabled.load()) || (cancelled && cancelled(context))) return;
    const uint32_t elapsed = millis() - started;
    const TickType_t wait = pdMS_TO_TICKS(std::min<uint32_t>(20,
        elapsed < timeoutMs ? timeoutMs - elapsed : 0));
    const EventBits_t required = BackgroundClosed | (control ? RadioClosed : 0);
    if (_foreground &&
        (xEventGroupWaitBits(idle, required, pdFALSE, pdTRUE, wait) & required) != required)
      continue;
    if (xSemaphoreTake(gate, wait) == pdTRUE) {
      if ((!control && !enabled.load()) || (cancelled && cancelled(context))) {
        xSemaphoreGive(gate);
        return;
      }
      // A map may have retained its connection while we waited for this mutex.
      // A driver transition also waits for the established radio to close.
      if ((_foreground && backgroundRetained.load()) ||
          (control && sharedSessions.load() != 0)) {
        xSemaphoreGive(gate);
        continue;
      }
      _locked = true;
      exclusiveBusy = true;
      return;
    }
  } while (millis() - started < timeoutMs);
  Tab5DebugSerial.printf("[NET] %s : connexion differee, autre ouverture en cours\n", owner);
}

void Tab5NetworkLease::shareAfterSetup() {
  if (!_locked) return;
  // Only the single radio owner can publish this state. Its receiver, codec,
  // TLS and speaker allocations are complete before another request starts.
  ++sharedSessions;
  xEventGroupClearBits(idle, RadioClosed);
  _shared = true;
  if (_foreground) { --foregroundSessions; _foreground = false; }
  _locked = false;
  exclusiveBusy = false;
  xSemaphoreGive(gate);
}

Tab5NetworkLease::~Tab5NetworkLease() {
  if (_locked) { exclusiveBusy = false; xSemaphoreGive(gate); }
  if (_shared && sharedSessions.fetch_sub(1) == 1)
    xEventGroupSetBits(idle, RadioClosed);
  if (_foreground) --foregroundSessions;
}
