#pragma once
#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace Tab5MapSearch {
// Preserve complete UTF-8 characters in bounded, display-owned strings.
inline void copy(char* out, size_t capacity, const char* source) {
  if (!capacity) return;
  if (!source) source = "";
  size_t n = std::strlen(source);
  if (n >= capacity) {
    n = capacity - 1;
    while (n && (static_cast<unsigned char>(source[n]) & 0xc0) == 0x80) --n;
  }
  std::memcpy(out, source, n); out[n] = 0;
}
inline bool coordinate(const char* text, double limit, double& value) {
  if (!text || !*text) return false;
  char* end = nullptr;
  const double parsed = std::strtod(text, &end);
  if (end == text || *end || !std::isfinite(parsed) || parsed < -limit || parsed > limit) return false;
  value = parsed; return true;
}
inline uint8_t zoom(int rank) {
  return rank <= 8 ? 5 : rank <= 12 ? 7 : rank <= 16 ? 10 : rank <= 20 ? 12 : rank <= 24 ? 14 : 16;
}
// The provider can return hundreds of translated names and address fields.
// Parse only the few fields displayed by Cartes, never a full second copy of
// that payload. The caller supplies bounded PSRAM output and allocator.
// Negative result: -1 malformed response, -2 allocation failure.
template<class Item>
int decode(const uint8_t* data, size_t size, const char* language,
           Item* output, size_t capacity, ArduinoJson::Allocator* allocator) {
  if (!data || !size || !output || !capacity) return -1;
  char localized[40] = {}, primary[40] = {};
  std::snprintf(localized, sizeof(localized), "name:%s", language ? language : "en-US");
  copy(primary, sizeof(primary), localized);
  if (char* dash = std::strchr(primary, '-')) *dash = 0;
  ArduinoJson::JsonDocument filter(allocator);
  auto fields = filter[0];
  for (const char* key : {"lat", "lon", "name", "display_name", "place_rank"}) fields[key] = true;
  fields["namedetails"]["name"] = true;
  fields["namedetails"]["alt_name"] = true;
  fields["namedetails"][localized] = true;
  fields["namedetails"][primary] = true;
  if (filter.overflowed()) return -2;
  ArduinoJson::JsonDocument document(allocator);
  const auto error = ArduinoJson::deserializeJson(document, data, size,
      ArduinoJson::DeserializationOption::Filter(filter),
      ArduinoJson::DeserializationOption::NestingLimit(6));
  if (error) return error == ArduinoJson::DeserializationError::NoMemory ? -2 : -1;
  if (!document.is<ArduinoJson::JsonArrayConst>()) return -1;
  size_t count = 0;
  for (auto entry : document.as<ArduinoJson::JsonArrayConst>()) {
    if (count >= capacity) break;
    double latitude, longitude;
    if (!coordinate(entry["lat"] | "", 90, latitude) ||
        !coordinate(entry["lon"] | "", 180, longitude)) continue;
    auto& item = output[count]; item = {};
    item.latitude = latitude; item.longitude = longitude;
    item.zoom = zoom(entry["place_rank"] | 24);
    auto names = entry["namedetails"];
    const char* name = names[localized] | "";
    if (!*name) name = names[primary] | "";
    if (!*name) name = entry["name"] | "";
    if (!*name) name = names["name"] | "";
    const char* display = entry["display_name"] | name;
    copy(item.name, sizeof(item.name), *name ? name : display);
    copy(item.displayName, sizeof(item.displayName), display);
    copy(item.alternative, sizeof(item.alternative), names["alt_name"] | "");
    ++count;
  }
  return static_cast<int>(count);
}
}
