#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace Tab5Reader {
// Sidecars are optional and untrusted. Read in blocks and stop on a damaged
// record instead of draining an arbitrarily large line on the UI task.
class Lines {
 public:
  using Read = size_t (*)(void*, uint32_t, uint8_t*, size_t);
  using Yield = void (*)();
  static constexpr uint32_t MaxBytes = 4U * 1024 * 1024;
  Lines(void* context, Read read, uint32_t size, uint32_t start = 0, Yield yield = nullptr)
      : context_(context), read_(read), yield_(yield), size_(size), offset_(start) {
    if (size > MaxBytes || start > size || !read) stopped_ = true;
  }
  uint32_t position() const { return offset_; }
  bool next(char* text, size_t capacity) {
    if (stopped_ || offset_ >= size_ || !text || !capacity) return false;
    size_t used = 0, length = 0; bool overflow = false;
    while (offset_ < size_) {
      if (at_ == available_) {
        available_ = std::min<size_t>(sizeof(buffer_), size_ - offset_);
        if (read_(context_, offset_, buffer_, available_) != available_) {
          stopped_ = true; text[0] = 0; return false;
        }
        at_ = 0;
        if (yield_) yield_();
      }
      const uint8_t c = buffer_[at_++]; ++offset_;
      if (c == '\n') break;
      if (++length > 4096 || c == 0) { stopped_ = true; text[0] = 0; return false; }
      if (c == '\r') continue;
      if (used + 1 < capacity) text[used++] = char(c); else overflow = true;
    }
    text[overflow ? 0 : used] = 0;
    return true;
  }
 private:
  void* context_; Read read_; Yield yield_;
  uint32_t size_, offset_; size_t at_ = 0, available_ = 0;
  bool stopped_ = false; uint8_t buffer_[512];
};
}
