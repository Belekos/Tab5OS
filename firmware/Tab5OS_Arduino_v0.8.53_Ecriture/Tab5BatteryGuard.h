#pragma once
#include <stdint.h>

// Tab5 NP-F550 (2S) protection policy. This supplements the pack's protection;
// it neither changes the charger settings nor estimates a calibrated state of charge.
namespace Tab5BatteryGuard {
struct Sample {
  bool valid = false;
  bool charging = false;
  int32_t millivolts = 0;
  int32_t milliamps = 0; // Positive = charge, negative = discharge.
};
enum class Event : uint8_t { None, Low, Countdown, Cancelled, Shutdown };
struct Result {
  Event event = Event::None;
  Sample sample;
  uint32_t seconds = 0;
  bool countdown = false;
};

class Policy {
 public:
  static constexpr uint32_t samplePeriodMs = 1000;
  static constexpr uint32_t maxSampleGapMs = 2500;
  static constexpr uint32_t chargingHoldMs = 30000;
  static constexpr int32_t warningMv = 7000;
  static constexpr int32_t criticalMv = 6800;
  static constexpr int32_t urgentMv = 6400;
  static constexpr int32_t recoverMv = 6950;

  static bool discharging(const Sample& s) {
    return s.valid && !s.charging && s.millivolts >= 4500 &&
           s.millivolts <= 8800 && s.milliamps <= -25 && s.milliamps >= -8000;
  }
  Result update(uint32_t now, const Sample& s) {
    Result result; result.sample = s;
    const bool interrupted = seen_ && uint32_t(now - previous_) > maxSampleGapMs;
    previous_ = now; seen_ = true;
    if (s.valid && (s.charging || s.milliamps >= 25)) {
      chargingSeen_ = true; lastCharging_ = now;
    }
    const bool chargeSettling = chargingSeen_ && uint32_t(now - lastCharging_) < chargingHoldMs;
    // Do not carry a countdown over an I2C failure, charging/USB recovery, absent
    // battery, or a blocked UI loop. Require fresh continuous observations again.
    if (interrupted || chargeSettling || !discharging(s) || s.millivolts >= recoverMv) {
      const bool cancelled = countdown_;
      critical_.reset(); urgent_.reset();
      countdown_ = false; ready_ = false;
      if (s.valid && (s.charging || s.millivolts > 7150)) warned_ = false;
      if (cancelled) result.event = Event::Cancelled;
      // A sample between 6.95 V and 7 V may still produce the low warning below.
      if (interrupted || chargeSettling || !discharging(s) || s.millivolts > warningMv) {
        low_.reset(); return result;
      }
    }
    if (s.millivolts <= warningMv) low_.observe(now); else low_.reset();
    if (s.millivolts <= criticalMv) critical_.observe(now); else critical_.reset();
    if (s.millivolts <= urgentMv) urgent_.observe(now); else urgent_.reset();

    const bool urgent = urgent_.elapsed(now, 3000, 4);
    const bool critical = critical_.elapsed(now, 10000, 11);
    if (!countdown_ && (urgent || critical)) {
      countdown_ = true; ready_ = false;
      deadline_ = now + (urgent ? 5000u : 30000u);
    } else if (countdown_ && urgent && int32_t(deadline_ - now) > 5000) {
      deadline_ = now + 5000;
    }
    if (countdown_) {
      const int32_t remaining = int32_t(deadline_ - now);
      ready_ = remaining <= 0;
      result.countdown = true;
      result.seconds = remaining > 0 ? (uint32_t(remaining) + 999) / 1000 : 0;
      result.event = ready_ ? Event::Shutdown : Event::Countdown;
    } else if (result.event == Event::None && !warned_ && low_.elapsed(now, 15000, 16)) {
      warned_ = true; result.event = Event::Low;
    }
    return result;
  }
  bool ready() const { return ready_; }
 private:
  struct Duration {
    uint32_t start = 0;
    uint16_t count = 0;
    void reset() { count = 0; }
    void observe(uint32_t now) { if (!count) start = now; if (count < 65535) ++count; }
    bool elapsed(uint32_t now, uint32_t duration, uint16_t samples) const {
      return count >= samples && uint32_t(now - start) >= duration;
    }
  } low_, critical_, urgent_;
  uint32_t previous_ = 0, deadline_ = 0;
  uint32_t lastCharging_ = 0;
  bool chargingSeen_ = false;
  bool seen_ = false, warned_ = false, countdown_ = false, ready_ = false;
};

// UI task only. Reads are throttled to once a second; no allocation or SD writes.
// No batteryInstalled preference: an observed discharge itself confirms a pack.
Result service(uint32_t now);
const Result& status();
// Re-read both sensors immediately before requesting hardware power-off.
// Returns false if shutdown was cancelled. Normally does not return on success.
bool powerOffIfStillCritical(uint32_t now);
}
