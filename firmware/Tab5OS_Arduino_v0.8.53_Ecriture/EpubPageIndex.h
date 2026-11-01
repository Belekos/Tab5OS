#pragma once
#ifndef TAB5_EPUB_PAGE_INDEX_H
#define TAB5_EPUB_PAGE_INDEX_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// The book index stores measured chapter page counts, never an estimated count.
// Until every section is measured, only progress is estimated and labelled ~.
namespace epub_pages {
struct Geometry { int width; int height; int x; int y; };
inline Geometry geometry(int screenW, int screenH, float aspect) {
  if (aspect < 0.25f || aspect > 4.0f) aspect = 2.0f / 3.0f;
  const int zoneH = screenH - 128;
  int w = screenW, h = static_cast<int>(w / aspect + 0.5f);
  if (h > zoneH - 8) { h = zoneH - 8; w = static_cast<int>(h * aspect + 0.5f); }
  if (w < 280) w = 280;
  if (h < 360) h = 360;
  return {w, h, (screenW - w) / 2, 58 + (zoneH - h) / 2};
}

struct Status {
  uint32_t page = 0;
  uint32_t total = 0;
  uint8_t percent = 0;
  bool exact = false;
  bool pageKnown = false;
};

template <size_t Capacity> class Index {
 public:
  void reset(uint16_t count, const uint32_t* weights = nullptr) {
    count_ = count <= Capacity ? count : 0;
    memset(pages_, 0, sizeof(pages_));
    for (uint16_t i = 0; i < count_; ++i) weights_[i] = weights && weights[i] ? weights[i] : 1;
  }
  uint16_t count() const { return count_; }
  uint32_t pages(uint16_t chapter) const { return chapter < count_ ? pages_[chapter] : 0; }
  bool record(uint16_t chapter, uint32_t count) {
    if (chapter >= count_ || !count || count > 1000000U) return false;
    pages_[chapter] = count; return true;
  }
  uint16_t nextUnknown() const {
    for (uint16_t i = 0; i < count_; ++i) if (!pages_[i]) return i;
    return count_;
  }
  bool complete() const { return count_ && nextUnknown() == count_; }
  bool locate(uint32_t page, uint16_t& chapter, uint32_t& local) const {
    if (!complete() || !page) return false;
    for (uint16_t i = 0; i < count_; ++i) {
      if (page <= pages_[i]) { chapter = i; local = page - 1; return true; }
      page -= pages_[i];
    }
    return false;
  }
  Status status(uint16_t chapter, uint32_t localPage, uint64_t localDone, uint64_t localSize) const {
    Status s;
    if (chapter >= count_) return s;
    uint64_t before = 0, totalWeight = 0;
    uint32_t prefix = 0, totalPages = 0;
    bool prefixKnown = true, allKnown = true;
    for (uint16_t i = 0; i < count_; ++i) {
      totalWeight += weights_[i]; totalPages += pages_[i];
      if (!pages_[i]) allKnown = false;
      if (i < chapter) { before += weights_[i]; prefix += pages_[i]; if (!pages_[i]) prefixKnown = false; }
    }
    s.pageKnown = prefixKnown;
    s.page = prefixKnown ? prefix + localPage + 1 : 0;
    s.exact = allKnown;
    if (allKnown) {
      s.total = totalPages;
      if (s.page > s.total) s.page = s.total;
      s.percent = static_cast<uint8_t>((static_cast<uint64_t>(s.page) * 100U) / s.total);
    } else {
      // Work in millionths so a page change within a short chapter is retained.
      if (!localSize) localSize = 1;
      if (localDone > localSize) localDone = localSize;
      const uint64_t fraction = localDone * 1000000ULL / localSize;
      const uint64_t done = before * 1000000ULL + weights_[chapter] * fraction;
      s.percent = totalWeight ? static_cast<uint8_t>(done / (totalWeight * 10000ULL)) : 0;
      if (s.percent >= 100) s.percent = 99; // 100% requires the real last page.
    }
    return s;
  }
 private:
  uint16_t count_ = 0;
  uint32_t pages_[Capacity] = {};
  uint32_t weights_[Capacity] = {};
};
} // namespace epub_pages
#endif // TAB5_EPUB_PAGE_INDEX_H
