#pragma once
#include <lvgl.h>
#include <string>
#include <vector>

// Keep the end of a path visible. Work on UTF-8 boundaries and actual font
// widths, so LVGL never has to replace the meaningful suffix with dots.
inline std::string tailPath(const char* source, const lv_font_t* font, int width) {
  std::string text = source ? source : "";
  auto fits = [&](const std::string& value) {
    lv_point_t size;
    lv_text_get_size(&size, value.c_str(), font, 0, 0, 1000000, LV_TEXT_FLAG_NONE);
    return size.x <= width;
  };
  if (fits(text)) return text;
  const std::string prefix = "…";
  if (!fits(prefix)) return "";
  std::vector<size_t> boundaries;
  for (size_t i = 0; i < text.size(); ++i)
    if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) boundaries.push_back(i);
  boundaries.push_back(text.size());
  size_t lo = 0, hi = boundaries.size() - 1;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    if (fits(prefix + text.substr(boundaries[mid]))) hi = mid;
    else lo = mid + 1;
  }
  return prefix + text.substr(boundaries[lo]);
}
