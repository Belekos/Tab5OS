#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "Tab5MapViewPolicy.h"

// A complete view is committed as one NVS blob, never as independent values.
// Fixed bytes avoid structure padding; checksum rejects stale/corrupt payloads.
namespace Tab5MapViewState {
constexpr double DefaultLatitude = 0.0;
constexpr double DefaultLongitude = 0.0;
constexpr uint8_t DefaultZoom = 2;
constexpr double MaxLatitude = 85.05112878;
constexpr size_t RecordSize = 24;
struct Record { uint8_t bytes[RecordSize] = {}; };
static_assert(sizeof(double) == 8, "Map storage requires IEEE-754 doubles");

inline bool valid(double latitude, double longitude, uint8_t zoom) {
  return std::isfinite(latitude) && std::isfinite(longitude) &&
      latitude >= -MaxLatitude && latitude <= MaxLatitude &&
      longitude >= -180.0 && longitude < 180.0 &&
      zoom <= Tab5MapViewPolicy::MaxZoom;
}
inline uint32_t checksum(const Record& record) {
  uint32_t value = 2166136261u;
  for (size_t i = 0; i < RecordSize - 4; ++i)
    value = (value ^ record.bytes[i]) * 16777619u;
  return value;
}
inline bool encode(double latitude, double longitude, uint8_t zoom, Record& record) {
  if (!valid(latitude, longitude, zoom)) return false;
  record = {};
  record.bytes[0] = 1;  // record version
  record.bytes[1] = zoom;
  record.bytes[2] = 'M'; record.bytes[3] = 'V';
  std::memcpy(record.bytes + 4, &latitude, sizeof(latitude));
  std::memcpy(record.bytes + 12, &longitude, sizeof(longitude));
  const uint32_t sum = checksum(record);
  std::memcpy(record.bytes + 20, &sum, sizeof(sum));
  return true;
}
inline bool decode(const Record& record, double& latitude, double& longitude,
                   uint8_t& zoom) {
  uint32_t sum = 0;
  std::memcpy(&sum, record.bytes + 20, sizeof(sum));
  if (record.bytes[0] != 1 || record.bytes[2] != 'M' ||
      record.bytes[3] != 'V' || checksum(record) != sum) return false;
  double lat = 0, lon = 0;
  const uint8_t z = record.bytes[1];
  std::memcpy(&lat, record.bytes + 4, sizeof(lat));
  std::memcpy(&lon, record.bytes + 12, sizeof(lon));
  if (!valid(lat, lon, z)) return false;
  latitude = lat; longitude = lon; zoom = z;
  return true;
}
}  // namespace Tab5MapViewState
