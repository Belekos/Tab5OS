#pragma once
#include <stddef.h>
#include <stdint.h>
#include "WriterEncoding.h"

// All offsets are UTF-8 byte offsets. UI character positions are converted at
// the boundary. No LVGL, Arduino or filesystem dependency in the text engine.
namespace writing {
void* allocate(size_t n);
void release(void* p);
bool validUtf8(const char* text, size_t length);
size_t byteOffset(const char* text, size_t characters);
size_t charCount(const char* text, size_t length);

class Document {
 public:
  static constexpr size_t Limit = 2 * 1024 * 1024;
  static constexpr size_t HistoryLimit = 2 * 1024 * 1024;
  static constexpr size_t HistoryCount = 256;
  Document() = default;
  ~Document();
  Document(const Document&) = delete;
  Document& operator=(const Document&) = delete;
  void swap(Document& other);
  bool assign(const char* text, size_t length);
  bool appendLoading(const char* text, size_t length);
  bool finishLoading();
  size_t size() const { return size_; }
  char at(size_t offset) const;
  bool boundary(size_t offset) const;
  size_t copy(size_t offset, char* dest, size_t length) const;
  char* snapshot() const;
  bool replace(size_t start, size_t end, const char* text, size_t length,
               size_t beforeCursor, size_t afterCursor, uint32_t now = 0);
  bool undo(size_t& cursor);
  bool redo(size_t& cursor);
  bool canUndo() const { return historyPosition_ > 0; }
  bool canRedo() const { return historyPosition_ < historyCount_; }
  size_t historyBytes() const { return historyBytes_; }
  uint64_t revision() const { return revision_; }
  bool dirty() const { return revision_ != savedRevision_ || encoding_ != savedEncoding_; }
  TextEncoding encoding() const { return encoding_; }
  void setEncoding(TextEncoding value) { if (value != TextEncoding::Auto) encoding_ = value; }
  void markSaved(uint64_t revision) { markSaved(revision, encoding_); }
  void markSaved(uint64_t revision, TextEncoding encoding) { savedRevision_ = revision; savedEncoding_ = encoding; }
  // Search is literal, case-sensitive UTF-8; npos means no result.
  static constexpr size_t npos = static_cast<size_t>(-1);
  size_t find(const char* needle, size_t from = 0) const;
  bool replaceAll(const char* needle, const char* replacement, size_t& count);
  size_t pageEnd(size_t start, size_t maxBytes = 8192) const;
  size_t previousPage(size_t start, size_t maxBytes = 8192) const;
  size_t lineStart(size_t position) const;
  size_t nextChar(size_t position) const;
  size_t prevChar(size_t position) const;
  size_t words() const;
  size_t characters() const;
 private:
  struct Change {
    size_t offset = 0, removed = 0, inserted = 0;
    size_t beforeCursor = 0, afterCursor = 0;
    char* bytes = nullptr; // removed bytes, followed by inserted bytes
    uint64_t beforeRevision = 0, afterRevision = 0;
    uint32_t stamp = 0;
  };
  TextEncoding encoding_ = TextEncoding::Utf8, savedEncoding_ = TextEncoding::Utf8;
  char* data_ = nullptr;
  size_t size_ = 0, capacity_ = 0, gap_ = 0;
  Change* history_ = nullptr;
  size_t historyCount_ = 0, historyPosition_ = 0, historyBytes_ = 0;
  uint64_t revision_ = 0, nextRevision_ = 1, savedRevision_ = 0;
  bool ensure(size_t bytes);
  void moveGap(size_t position);
  bool rawReplace(size_t start, size_t end, const char* text, size_t length);
  void clearHistory();
  void dropFirst();
};
} // namespace writing
