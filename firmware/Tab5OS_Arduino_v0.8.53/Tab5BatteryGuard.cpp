#include "Tab5BatteryGuard.h"
#include <M5Unified.h>

namespace Tab5BatteryGuard { namespace {
Policy policy;
Result current;
bool sampled = false;
uint32_t previousSample = 0;

Sample readSample() {
  Sample result;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  // Deliberately limited to the original Tab5 whose schematic was checked.
  if (M5.getBoard() != m5::board_t::board_M5Tab5) return result;
  constexpr uint32_t frequency = 400000;
  uint8_t id[2] = {}, config[2] = {}, voltage[2] = {}, shunt[2] = {};
  uint8_t chargePins = 0;
  // The public voltage/current convenience getters turn failed reads into zero.
  // Keep the I2C success bit, check the chip and continuous-conversion mode.
  if (!M5.In_I2C.readRegister(0x41, 0xff, id, 2, frequency) ||
      id[0] != 0x22 || id[1] != 0x60 ||
      !M5.In_I2C.readRegister(0x41, 0x00, config, 2, frequency) ||
      (config[1] & 7) != 7 ||
      !M5.In_I2C.readRegister(0x41, 0x02, voltage, 2, frequency) ||
      !M5.In_I2C.readRegister(0x41, 0x01, shunt, 2, frequency) ||
      !M5.In_I2C.readRegister(0x44, 0x0f, &chargePins, 1, frequency)) return result;
  const uint16_t bus = uint16_t(uint16_t(voltage[0]) << 8) | voltage[1];
  const uint16_t raw = uint16_t(uint16_t(shunt[0]) << 8) | shunt[1];
  const int32_t signedShunt = (raw & 0x8000) ? int32_t(raw) - 65536 : raw;
  // INA226: 1.25 mV / bus unit; 2.5 uV / shunt unit across R39 = 5 mOhm.
  // Tab5 wiring makes discharge positive at the INA226; invert (+ = charge).
  result.millivolts = int32_t(bus) * 5 / 4;
  result.milliamps = -signedShunt / 2;
  // PI4IOE5V6408 input register, P6 = IP2326 CHG_STAT (same polarity as M5Unified).
  result.charging = (chargePins & (1u << 6)) != 0;
  result.valid = result.millivolts >= 4500 && result.millivolts <= 8800 &&
                 result.milliamps >= -8000 && result.milliamps <= 8000;
#endif
  return result;
}
} // namespace

Result service(uint32_t now) {
  if (sampled && uint32_t(now - previousSample) < Policy::samplePeriodMs) {
    Result unchanged = current; unchanged.event = Event::None; return unchanged;
  }
  previousSample = now; sampled = true;
  current = policy.update(now, readSample());
  return current;
}
const Result& status() { return current; }
bool powerOffIfStillCritical(uint32_t now) {
  if (!policy.ready()) return false;
  current = policy.update(now, readSample());
  previousSample = now; sampled = true;
  if (current.event != Event::Shutdown || !Policy::discharging(current.sample)) return false;
  // M5Unified Tab5 sends PWROFF_PLUSE to the hardware supervisor. The library
  // uses deep sleep only as its fallback if hardware power removal did not occur.
  M5.Power.powerOff();
  return true;
}
}
