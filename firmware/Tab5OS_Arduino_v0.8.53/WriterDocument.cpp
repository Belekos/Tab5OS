#include "WriterDocument.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif

namespace writing {
void* allocate(size_t n) {
#if defined(ARDUINO_ARCH_ESP32)
  return heap_caps_malloc(n ? n : 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return malloc(n ? n : 1);
#endif
}
void release(void* p) {
#if defined(ARDUINO_ARCH_ESP32)
  heap_caps_free(p);
#else
  free(p);
#endif
}
bool validUtf8(const char* s, size_t n) {
  for (size_t i = 0; i < n;) {
    const uint8_t c = static_cast<uint8_t>(s[i++]);
    if (!c) return false; // Embedded NUL cannot be represented by LVGL.
    if (c < 0x80) continue;
    unsigned rest; uint32_t cp, minimum;
    if (c >= 0xc2 && c <= 0xdf) { rest = 1; cp = c & 31; minimum = 0x80; }
    else if (c >= 0xe0 && c <= 0xef) { rest = 2; cp = c & 15; minimum = 0x800; }
    else if (c >= 0xf0 && c <= 0xf4) { rest = 3; cp = c & 7; minimum = 0x10000; }
    else return false;
    if (n - i < rest) return false;
    while (rest--) {
      uint8_t b = static_cast<uint8_t>(s[i++]);
      if ((b & 0xc0) != 0x80) return false;
      cp = (cp << 6) | (b & 63);
    }
    if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
  }
  return true;
}
size_t byteOffset(const char* s, size_t chars) {
  size_t i = 0;
  while (s[i] && chars--) { ++i; while ((static_cast<uint8_t>(s[i]) & 0xc0) == 0x80) ++i; }
  return i;
}
size_t charCount(const char* s, size_t n) {
  size_t count = 0;
  for (size_t i = 0; i < n; ++i) if ((static_cast<uint8_t>(s[i]) & 0xc0) != 0x80) ++count;
  return count;
}
Document::~Document() { clearHistory(); release(history_); release(data_); }
void Document::swap(Document& o) {
  using std::swap;
  swap(encoding_, o.encoding_); swap(savedEncoding_, o.savedEncoding_);
  swap(data_, o.data_); swap(size_, o.size_); swap(capacity_, o.capacity_); swap(gap_, o.gap_);
  swap(history_, o.history_); swap(historyCount_, o.historyCount_);
  swap(historyPosition_, o.historyPosition_); swap(historyBytes_, o.historyBytes_);
  swap(revision_, o.revision_); swap(nextRevision_, o.nextRevision_); swap(savedRevision_, o.savedRevision_);
}
char Document::at(size_t pos) const {
  return pos < size_ ? data_[pos < gap_ ? pos : pos + capacity_ - size_] : 0;
}
bool Document::boundary(size_t p) const {
  return p <= size_ && (p == size_ || (static_cast<uint8_t>(at(p)) & 0xc0) != 0x80);
}
size_t Document::copy(size_t offset, char* dest, size_t n) const {
  if (offset >= size_) return 0;
  n = std::min(n, size_ - offset);
  const size_t before = offset < gap_ ? std::min(n, gap_ - offset) : 0;
  if (before) memcpy(dest, data_ + offset, before);
  if (n > before) memcpy(dest + before, data_ + offset + before + capacity_ - size_, n - before);
  return n;
}
char* Document::snapshot() const {
  char* p = static_cast<char*>(allocate(size_ + 1));
  if (p) { copy(0, p, size_); p[size_] = 0; }
  return p;
}
bool Document::ensure(size_t bytes) {
  if (bytes > Limit) return false;
  if (bytes <= capacity_) return true;
  size_t cap = capacity_ ? capacity_ : 4096;
  while (cap < bytes) cap = std::min(Limit, cap * 2);
  char* p = static_cast<char*>(allocate(cap));
  if (!p) return false;
  if (gap_) memcpy(p, data_, gap_);
  const size_t tail = size_ - gap_;
  if (tail) memcpy(p + cap - tail, data_ + capacity_ - tail, tail);
  release(data_); data_ = p; capacity_ = cap;
  return true;
}
void Document::moveGap(size_t p) {
  const size_t gapSize = capacity_ - size_;
  if (p < gap_) memmove(data_ + p + gapSize, data_ + p, gap_ - p);
  else if (p > gap_) memmove(data_ + gap_, data_ + gap_ + gapSize, p - gap_);
  gap_ = p;
}
bool Document::rawReplace(size_t a, size_t b, const char* s, size_t n) {
  if (!ensure(size_ - (b - a) + n)) return false;
  if (capacity_) moveGap(a);
  size_ -= b - a;
  if (n) memcpy(data_ + gap_, s, n);
  gap_ += n; size_ += n;
  return true;
}
void Document::clearHistory() {
  for (size_t i = 0; i < historyCount_; ++i) release(history_[i].bytes);
  historyCount_ = historyPosition_ = historyBytes_ = 0;
}
void Document::dropFirst() {
  historyBytes_ -= history_[0].removed + history_[0].inserted;
  release(history_[0].bytes);
  memmove(history_, history_ + 1, (--historyCount_) * sizeof(Change));
  if (historyPosition_) --historyPosition_;
}
bool Document::assign(const char* s, size_t n) {
  if (n > Limit || !validUtf8(s, n)) return false;
  Document candidate;
  if (!candidate.appendLoading(s, n)) return false;
  swap(candidate);
  return true;
}
bool Document::appendLoading(const char* s, size_t n) {
  if (n > Limit - size_ || !ensure(size_ + n)) return false;
  if (capacity_) moveGap(size_);
  if (n) memcpy(data_ + gap_, s, n);
  gap_ += n; size_ += n;
  return true;
}
bool Document::finishLoading() {
  // appendLoading leaves the gap at EOF, so validation needs no second buffer.
  return validUtf8(data_, size_);
}
bool Document::replace(size_t a, size_t b, const char* s, size_t n,
                       size_t before, size_t after, uint32_t now) {
  if (a > b || b > size_ || !boundary(a) || !boundary(b) || !validUtf8(s, n) || n > Limit - (size_ - (b-a))) return false;
  if (a == b && !n) return true;
  const size_t removed = b - a, cost = removed + n;
  // A change must be undoable. Refuse instead of silently dropping undo.
  if (cost > HistoryLimit) return false;
  if (!history_) {
    history_ = static_cast<Change*>(allocate(HistoryCount * sizeof(Change)));
    if (!history_) return false;
  }
  char* payload = static_cast<char*>(allocate(cost));
  if (!payload) return false;
  copy(a, payload, removed);
  if (n) memcpy(payload + removed, s, n);
  if (!ensure(size_ - removed + n)) { release(payload); return false; }
  while (historyCount_ > historyPosition_) {
    auto& c = history_[--historyCount_]; historyBytes_ -= c.removed + c.inserted; release(c.bytes);
  }
  // Coalesce adjacent typing for 900 ms, with a 4 KiB maximum group. Never
  // coalesce across a saved revision (undo must be able to return to it).
  if (now && historyPosition_ && !removed && n && revision_ != savedRevision_) {
    auto& c = history_[historyPosition_ - 1];
    if (!c.removed && c.offset + c.inserted == a && now - c.stamp < 900 && c.inserted + n <= 4096 && historyBytes_ + n <= HistoryLimit) {
      char* merged = static_cast<char*>(allocate(c.inserted + n));
      if (merged) {
        memcpy(merged, c.bytes, c.inserted); memcpy(merged + c.inserted, s, n);
        rawReplace(a, b, s, n); release(c.bytes); c.bytes = merged; c.inserted += n;
        c.afterCursor = after; c.stamp = now; c.afterRevision = nextRevision_++;
        revision_ = c.afterRevision; historyBytes_ += n; release(payload); return true;
      }
    }
  }
  while (historyCount_ && (historyCount_ >= HistoryCount || historyBytes_ + cost > HistoryLimit)) dropFirst();
  Change c;
  c.offset = a; c.removed = removed; c.inserted = n; c.bytes = payload;
  c.beforeCursor = before; c.afterCursor = after; c.stamp = now;
  c.beforeRevision = revision_; c.afterRevision = nextRevision_++;
  rawReplace(a, b, s, n);
  history_[historyCount_++] = c; historyPosition_ = historyCount_; historyBytes_ += cost;
  revision_ = c.afterRevision;
  return true;
}
bool Document::undo(size_t& cursor) {
  if (!canUndo()) return false;
  auto& c = history_[historyPosition_ - 1];
  if (!rawReplace(c.offset, c.offset + c.inserted, c.bytes, c.removed)) return false;
  --historyPosition_; revision_ = c.beforeRevision; cursor = c.beforeCursor; return true;
}
bool Document::redo(size_t& cursor) {
  if (!canRedo()) return false;
  auto& c = history_[historyPosition_];
  if (!rawReplace(c.offset, c.offset + c.removed, c.bytes + c.removed, c.inserted)) return false;
  ++historyPosition_; revision_ = c.afterRevision; cursor = c.afterCursor; return true;
}
size_t Document::find(const char* needle, size_t from) const {
  size_t n = strlen(needle);
  if (!n || n > size_ || from > size_ - n || !validUtf8(needle, n)) return npos;
  // BMH skip table; search across the gap without flattening the manuscript.
  size_t skip[256]; std::fill(skip, skip + 256, n);
  for (size_t i = 0; i + 1 < n; ++i) skip[static_cast<uint8_t>(needle[i])] = n - i - 1;
  for (size_t p = from; p <= size_ - n;) {
    size_t j = n;
    while (j && at(p + j - 1) == needle[j - 1]) --j;
    if (!j) return p;
    p += skip[static_cast<uint8_t>(at(p + n - 1))];
  }
  return npos;
}
bool Document::replaceAll(const char* needle, const char* repl, size_t& count) {
  count = 0;
  const size_t n = strlen(needle), r = strlen(repl);
  if (!n || !validUtf8(needle, n) || !validUtf8(repl, r)) return false;
  size_t p = 0, pos;
  while ((pos = find(needle, p)) != npos) { ++count; p = pos + n; }
  if (!count) return true;
  if (r > n && count > (Limit - size_) / (r - n)) return false;
  const size_t nextSize = size_ - count * n + count * r;
  if (size_ + nextSize > HistoryLimit) return false;
  char* out = static_cast<char*>(allocate(nextSize + 1));
  if (!out) return false;
  p = 0; size_t written = 0;
  while ((pos = find(needle, p)) != npos) {
    written += copy(p, out + written, pos - p);
    if (r) memcpy(out + written, repl, r);
    written += r; p = pos + n;
  }
  written += copy(p, out + written, size_ - p); out[written] = 0;
  bool ok = replace(0, size_, out, written, 0, 0); release(out); return ok;
}
size_t Document::nextChar(size_t p) const { if (p < size_) ++p; while (p < size_ && !boundary(p)) ++p; return p; }
size_t Document::prevChar(size_t p) const { p = std::min(p, size_); if (p) --p; while (p && !boundary(p)) --p; return p; }
size_t Document::lineStart(size_t p) const { p = std::min(p, size_); while (p && at(p-1) != '\n') --p; return p; }
size_t Document::pageEnd(size_t start, size_t max) const {
  if (start >= size_) return size_;
  size_t end = start + std::min(max, size_ - start);
  if (end == size_) return end;
  const size_t floor = start + (end - start) / 2;
  for (size_t p = end; p > floor; --p) if (at(p-1) == '\n') return p;
  while (end > start && !boundary(end)) --end;
  return end;
}
size_t Document::previousPage(size_t start, size_t max) const {
  size_t p = 0, previous = 0;
  while (p < start) { previous = p; p = pageEnd(p, max); if (p <= previous) break; }
  return previous;
}
size_t Document::words() const {
  size_t n = 0; bool word = false;
  for (size_t i = 0; i < size_; ++i) {
    const uint8_t c = static_cast<uint8_t>(at(i));
    const bool space = c <= 32 || (c == 0xc2 && static_cast<uint8_t>(at(i+1)) == 0xa0) ||
        (c == 0xe2 && static_cast<uint8_t>(at(i+1)) == 0x80 && static_cast<uint8_t>(at(i+2)) == 0xaf);
    if (space) { word = false; if (c == 0xc2) ++i; else if (c == 0xe2) i += 2; }
    else if ((c & 0xc0) != 0x80 && !word) { ++n; word = true; }
  }
  return n;
}
size_t Document::characters() const {
  size_t n = 0; for (size_t i = 0; i < size_; ++i) if (boundary(i)) ++n; return n;
}
} // namespace writing
