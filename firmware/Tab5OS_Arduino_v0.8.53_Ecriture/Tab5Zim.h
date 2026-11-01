#pragma once
#include <stddef.h>
#include <stdint.h>

namespace Tab5Zim {
struct Source {
  void* context = nullptr;
  uint64_t size = 0;
  bool (*readAt)(void*, uint64_t, uint8_t*, size_t) = nullptr;
};
struct Callbacks {
  void* context = nullptr;
  bool (*cancelled)(void*) = nullptr;
  void (*cooperate)(void*) = nullptr;
  void* (*allocate)(void*, size_t) = nullptr;
  void (*release)(void*, void*) = nullptr;
};
struct Entry {
  uint32_t id = 0, cluster = 0, blob = 0, redirect = UINT32_MAX;
  uint16_t mime = 0;
  char nameSpace = 0;
  char path[1024] = {}, title[1024] = {};
};
struct Sink {
  void* context = nullptr;
  bool (*write)(void*, const uint8_t*, size_t) = nullptr;
};
// Offsets address the UTF-8 bytes emitted to the text sink. Targets include the
// ZIM namespace (for example C/Paris), never a network URL or a local SD path.
struct RichOptions {
  Sink links; // start<TAB>end<TAB>target<LF>, with an exclusive end offset.
  void* context = nullptr;
  bool (*image)(void*, uint64_t offset, const char* target) = nullptr;
  unsigned maxImages = 8;
};
using SearchCallback = bool (*)(void*, const Entry&);

class Reader {
 public:
  explicit Reader(Source source, Callbacks callbacks = {});
  bool open();
  bool entry(uint32_t id, Entry& result);
  bool search(const char* prefix, SearchCallback callback, void* context,
              unsigned limit = 30, unsigned* found = nullptr,
              uint32_t cursor = 0, uint32_t* nextCursor = nullptr);
  bool extractText(uint32_t id, Sink sink, Entry* resolved = nullptr,
                   uint64_t* bytesWritten = nullptr, const RichOptions* rich = nullptr);
  bool resolveLink(const char* link, Entry& result, const char* base = nullptr);
  bool extractImage(uint32_t id, Sink sink, uint64_t* bytesWritten = nullptr,
                    uint64_t limit = 131072);
  const char* error() const { return error_; }
  uint32_t entryCount() const { return entries_; }
  uint32_t articleCount() const { return titles_; }
  uint64_t size() const { return source_.size; }
  uint32_t mainEntry() const { return main_; }
  bool cooperate();
  void* allocate(size_t n);
  void release(void* p);
 private:
  Source source_;
  Callbacks callbacks_;
  const char* error_ = nullptr;
  uint64_t pathsAt_ = 0, titlesAt_ = 0, clustersAt_ = 0, mimeAt_ = 0, checksumAt_ = 0;
  uint32_t entries_ = 0, clusters_ = 0, titles_ = 0, main_ = UINT32_MAX;
  uint32_t lastYield_ = 0;
  bool ready_ = false;
  char contentNamespace_ = 'C';
  bool fail(const char* reason);
  bool readAt(uint64_t at, uint8_t* data, size_t n);
  bool readNumber(uint64_t at, unsigned width, uint64_t& value);
  bool stringAt(uint64_t& at, char* text, size_t capacity);
  bool findPath(char ns, const char* path, uint32_t& id, Entry& scratch);
  bool titleEntry(uint32_t at, Entry& result);
  bool clusterRange(uint32_t cluster, uint64_t& start, uint64_t& end, uint8_t& info);
  bool plainBlob(const Entry& item, uint64_t& start, uint64_t& size);
  bool mimeType(uint16_t index, char* text, size_t capacity);
  friend class ClusterStream;
};

#ifdef ARDUINO
} // namespace Tab5Zim
#include <Arduino.h>
#include <FS.h>
namespace Tab5Zim {
// One SD handle is reused across fixed-size segments; all archive offsets are 64-bit.
class SdSource {
 public:
  SdSource();
  ~SdSource();
  SdSource(const SdSource&) = delete;
  SdSource& operator=(const SdSource&) = delete;
  bool open(fs::FS& fs, const String& path, String& error);
  Source source() const;
  void close();
 private:
  struct State;
  State* state_ = nullptr;
  static bool read(void*, uint64_t, uint8_t*, size_t);
};
#endif
} // namespace Tab5Zim
