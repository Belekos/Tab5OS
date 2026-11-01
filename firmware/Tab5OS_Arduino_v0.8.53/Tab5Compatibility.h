#pragma once

#include "Tab5CompatibilityPolicy.h"

namespace Tab5Compatibility {

// Call once after M5.begin(), before the UI loop starts. Uses the touch device
// selected by M5GFX; never changes panel timings, reset pins, or touch firmware.
void begin();

// Audio/network worker only. Register passive transport monitoring before
// WiFi.mode(). No firmware-version RPC and no compatibility admission check.
void beforeWifiStart();

// UI-safe cached data. These calls do not touch I2C, SDIO, or the C6 RPC layer.
Snapshot snapshot();
const char* displayName(Display display);
const char* firmwareMessage(Firmware firmware);
const char* transportMessage(Transport transport);

}  // namespace Tab5Compatibility
