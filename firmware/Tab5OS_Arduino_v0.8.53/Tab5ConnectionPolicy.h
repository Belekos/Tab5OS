#pragma once
#include <stdint.h>

// Connection decisions are independent from the media decoder and tested
// with simulated time, including the wrap of millis().
namespace Tab5Connection {
enum class WifiAction { Ready, Wait, Start };
inline WifiAction forStation(bool connected, bool connecting) {
  return connected ? WifiAction::Ready : (connecting ? WifiAction::Wait : WifiAction::Start);
}
inline uint32_t retryDelay(uint8_t attempt) {
  return attempt == 0 ? 3000U : (attempt == 1 ? 10000U : 30000U);
}
inline bool elapsed(uint32_t now, uint32_t since, uint32_t duration) {
  return static_cast<uint32_t>(now - since) >= duration;
}
constexpr uint8_t MaxRadioRetries = 3;
constexpr uint32_t StablePlaybackMs = 60000U;
}  // namespace Tab5Connection
