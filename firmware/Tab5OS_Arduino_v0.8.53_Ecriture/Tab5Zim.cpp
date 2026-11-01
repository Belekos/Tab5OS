#include "Tab5Zim.h"
#include "Tab5WikiTable.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <new>
#define ZSTD_STATIC_LINKING_ONLY
#include "src/zstd/zstd.h"
#include "src/zstd/zstd_errors.h"
#define XZ_USE_CRC64
extern "C" {
#include "src/xz/xz.h"
}
#ifdef ARDUINO_ARCH_ESP32
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#ifdef ARDUINO
#include <ArduinoJson.h>
#include "Tab5Storage.h"
#endif

namespace Tab5Zim {
namespace {
constexpr size_t InputBytes = 32768, OutputBytes = 8192;
constexpr uint64_t MaxClusterScan = 256ULL * 1024 * 1024;
constexpr uint64_t MaxArticleBytes = 64ULL * 1024 * 1024;
constexpr size_t MaxDecoderBytes = 12U * 1024 * 1024;
constexpr uint64_t PartBytes = 64ULL * 1024 * 1024;
uint64_t little(const uint8_t* p, unsigned n) {
  uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i) v |= uint64_t(p[i]) << (i * 8);
  return v;
}
bool range(uint64_t at, uint64_t size, uint64_t end) {
  return at <= end && size <= end - at;
}
}
Reader::Reader(Source source, Callbacks callbacks) : source_(source), callbacks_(callbacks) {}
bool Reader::fail(const char* reason) { if (!error_) error_ = reason; return false; }
bool Reader::cooperate() {
  if (callbacks_.cooperate) callbacks_.cooperate(callbacks_.context);
#ifdef ARDUINO_ARCH_ESP32
  else {
    uint32_t now = millis();
    if (uint32_t(now - lastYield_) >= 4) { lastYield_ = now; vTaskDelay(1); }
  }
#endif
  if (callbacks_.cancelled && callbacks_.cancelled(callbacks_.context)) return fail("Cancelled");
  return true;
}
void* Reader::allocate(size_t n) {
  if (callbacks_.allocate) return callbacks_.allocate(callbacks_.context, n);
#ifdef ARDUINO_ARCH_ESP32
  return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return std::malloc(n);
#endif
}
void Reader::release(void* p) {
  if (callbacks_.release) callbacks_.release(callbacks_.context, p);
#ifdef ARDUINO_ARCH_ESP32
  else heap_caps_free(p);
#else
  else std::free(p);
#endif
}
bool Reader::readAt(uint64_t at, uint8_t* data, size_t n) {
  if (!range(at, n, source_.size)) return fail("Invalid ZIM offset");
  // Even the legacy compatibility path must not issue a multi-megabyte SD
  // read between two cancellation checks.
  while(n) {
    if (!cooperate()) return false;
    const size_t take=std::min(InputBytes,n);
    if (!source_.readAt || !source_.readAt(source_.context, at, data, take)) return fail("Cannot read ZIM archive");
    at+=take;data+=take;n-=take;
  }
  return true;
}
bool Reader::readNumber(uint64_t at, unsigned width, uint64_t& value) {
  uint8_t bytes[8];
  if ((width != 4 && width != 8) || !readAt(at, bytes, width)) return false;
  value = little(bytes, width); return true;
}
bool Reader::stringAt(uint64_t& at, char* text, size_t capacity) {
  uint8_t chunk[128]; size_t used = 0;
  while (at < source_.size && used + 1 < capacity) {
    size_t n = size_t(std::min<uint64_t>(sizeof(chunk), source_.size - at));
    if (!readAt(at, chunk, n)) return false;
    for (size_t i = 0; i < n; ++i) {
      ++at;
      if (!chunk[i]) { text[used] = 0; return true; }
      if (used + 1 == capacity) return fail("ZIM title or path is too long");
      text[used++] = char(chunk[i]);
    }
  }
  return fail("Invalid ZIM string");
}
bool Reader::entry(uint32_t id, Entry& result) {
  if (id >= entries_) return fail("Invalid ZIM entry");
  uint64_t at;
  if (!readNumber(pathsAt_ + uint64_t(id) * 8, 8, at)) return false;
  uint8_t bytes[16];
  if (!readAt(at, bytes, 8)) return false;
  result.id = id; result.cluster = 0; result.blob = 0; result.redirect = UINT32_MAX;
  result.path[0] = result.title[0] = 0;
  result.mime = uint16_t(little(bytes, 2)); result.nameSpace = char(bytes[3]);
  at += 8;
  if (result.mime == 0xffff) {
    if (!readAt(at, bytes, 4)) return false;
    result.redirect = uint32_t(little(bytes, 4)); at += 4;
    if (result.redirect >= entries_) return fail("Invalid ZIM redirect");
  } else if (result.mime < 0xfffd) {
    if (!readAt(at, bytes, 8)) return false;
    result.cluster = uint32_t(little(bytes, 4)); result.blob = uint32_t(little(bytes + 4, 4)); at += 8;
    if (result.cluster >= clusters_) return fail("Invalid ZIM cluster");
  }
  if (!stringAt(at, result.path, sizeof(result.path)) || !stringAt(at, result.title, sizeof(result.title))) return false;
  if (!result.title[0]) std::memcpy(result.title, result.path, std::strlen(result.path) + 1);
  return true;
}
bool Reader::findPath(char ns, const char* path, uint32_t& id, Entry& item) {
  uint32_t lo = 0, hi = entries_;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (!entry(mid, item)) return false;
    int comparison = int(uint8_t(item.nameSpace)) - int(uint8_t(ns));
    if (!comparison) comparison = std::strcmp(item.path, path);
    if (comparison < 0) lo = mid + 1; else hi = mid;
  }
  id = UINT32_MAX;
  if (lo < entries_) {
    if (!entry(lo, item)) return false;
    if (item.nameSpace == ns && !std::strcmp(item.path, path)) id = lo;
  }
  return true;
}
bool Reader::clusterRange(uint32_t cluster, uint64_t& start, uint64_t& end, uint8_t& info) {
  if (cluster >= clusters_ || !readNumber(clustersAt_ + uint64_t(cluster) * 8, 8, start)) return false;
  end = checksumAt_ ? checksumAt_ : source_.size;
  if (cluster + 1 < clusters_ && !readNumber(clustersAt_ + uint64_t(cluster + 1) * 8, 8, end)) return false;
  if (start < 80 || start >= end || end > source_.size) return fail("Invalid ZIM cluster range");
  return readAt(start, &info, 1);
}
bool Reader::plainBlob(const Entry& item, uint64_t& start, uint64_t& size) {
  uint64_t cluster, end, first, from, to; uint8_t info;
  if (!clusterRange(item.cluster, cluster, end, info)) return false;
  if ((info & 15) > 1) return fail("Compressed ZIM title index is unsupported");
  const unsigned width = (info & 16) ? 8 : 4;
  const uint64_t base = cluster + 1;
  if (!readNumber(base, width, first)) return false;
  if (first < width * 2 || first % width || uint64_t(item.blob) + 1 >= first / width || !range(base, first, end))
    return fail("Invalid ZIM blob table");
  if (!readNumber(base + uint64_t(item.blob) * width, width, from) ||
      !readNumber(base + (uint64_t(item.blob) + 1) * width, width, to)) return false;
  if (from < first || to < from || !range(base, to, end)) return fail("Invalid ZIM blob range");
  start = base + from; size = to - from; return true;
}
bool Reader::open() {
  error_ = nullptr; ready_ = false;
  if (bool(callbacks_.allocate) != bool(callbacks_.release)) return fail("Invalid allocator callbacks");
  uint8_t bytes[80];
  if (!source_.readAt || source_.size < 96 || !readAt(0, bytes, sizeof(bytes))) return fail("Invalid ZIM archive");
  if (little(bytes, 4) != 0x044d495a) return fail("Not a ZIM archive (wrong signature or incomplete download)");
  const unsigned major=little(bytes+4,2);
  if(major!=5&&major!=6)return fail("Unsupported ZIM major version (expected 5 or 6)");
  contentNamespace_ = major>=6 && little(bytes + 6, 2) >= 1 ? 'C' : 'A';
  entries_ = uint32_t(little(bytes + 24, 4)); clusters_ = uint32_t(little(bytes + 28, 4));
  pathsAt_ = little(bytes + 32, 8); titlesAt_ = little(bytes + 40, 8);
  clustersAt_ = little(bytes + 48, 8); mimeAt_ = little(bytes + 56, 8);
  main_ = uint32_t(little(bytes + 64, 4)); checksumAt_ = mimeAt_==72?0:little(bytes + 72, 8);
  if (!entries_ || !clusters_ || (mimeAt_ != 80 && mimeAt_ != 72) ||
      !range(pathsAt_, uint64_t(entries_) * 8, source_.size) ||
      !range(clustersAt_, uint64_t(clusters_) * 8, source_.size) ||
      (checksumAt_ && (checksumAt_ > source_.size || source_.size - checksumAt_ != 16)))
    return fail("Incomplete or invalid ZIM archive");
  titles_ = 0;
  uint32_t listing; Entry item;
  if (!findPath('X', "listing/titleOrdered/v1", listing, item)) return false;
  if (listing != UINT32_MAX) {
    uint64_t bytesInIndex;
    if (!entry(listing, item) || item.redirect != UINT32_MAX || !plainBlob(item, titlesAt_, bytesInIndex)) return false;
    if (bytesInIndex % 4 || bytesInIndex / 4 > entries_) return fail("Invalid ZIM title index");
    titles_ = uint32_t(bytesInIndex / 4);
  } else {
    if (titlesAt_ == UINT64_MAX || !range(titlesAt_, uint64_t(entries_) * 4, source_.size))
      return fail("ZIM title index is missing");
    titles_ = entries_;
  }
  ready_ = true; return true;
}
bool Reader::titleEntry(uint32_t at, Entry& result) {
  uint64_t index;
  if (at >= titles_ || !readNumber(titlesAt_ + uint64_t(at) * 4, 4, index)) return false;
  return entry(uint32_t(index), result);
}
bool Reader::search(const char* prefix, SearchCallback callback, void* context, unsigned limit,
                    unsigned* found, uint32_t cursor, uint32_t* nextCursor) {
  error_ = nullptr;
  if (found) *found = 0;
  if (nextCursor) *nextCursor = 0;
  if (!ready_ || !prefix || !callback || !limit || limit > 100 || std::strlen(prefix) >= 1024)
    return fail("Invalid ZIM search");
  Entry item; uint32_t lo = cursor, hi = titles_;
  if (lo > titles_) return fail("Invalid ZIM search cursor");
  if (!cursor) {
    while (lo < hi) {
      uint32_t mid = lo + (hi - lo) / 2;
      if (!titleEntry(mid, item)) return false;
      int comparison = int(uint8_t(item.nameSpace)) - int(uint8_t(contentNamespace_));
      if (!comparison) comparison = std::strcmp(item.title, prefix);
      if (comparison < 0) lo = mid + 1; else hi = mid;
    }
  }
  unsigned count = 0; const size_t length = std::strlen(prefix);
  for (; lo < titles_; ++lo) {
    if (!titleEntry(lo, item)) return false;
    if (item.nameSpace != contentNamespace_ || std::strncmp(item.title, prefix, length)) break;
    if (item.mime >= 0xfffd && item.mime != 0xffff) continue;
    if (count == limit) { if (nextCursor) *nextCursor = lo; break; }
    if (!callback(context, item)) return fail("Cannot return ZIM search result");
    ++count;
  }
  if (found) *found = count;
  return true;
}
bool Reader::mimeType(uint16_t index, char* text, size_t capacity) {
  uint64_t at = mimeAt_; char value[192];
  for (uint32_t i = 0; i <= index; ++i) {
    if (!stringAt(at, value, sizeof(value))) return false;
    if (!value[0]) return fail("Invalid ZIM MIME type");
    if (i == index) {
      if (std::strlen(value) >= capacity) return fail("Invalid ZIM MIME type");
      std::strcpy(text, value); return true;
    }
  }
  return false;
}

namespace {
bool pathNamespace(char c) {
  return c == 'A' || c == 'C' || c == 'I' || c == 'J' || c == '-' || c == 'M' || c == 'W' || c == 'X';
}
int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
// Normalize one URL in a fixed buffer. A namespace is the first path component,
// so ../I/name and subdirectory-relative article links retain ZIM semantics.
bool canonicalLink(const char* link, const char* base, char fallback, char* out, size_t capacity) {
  if (!link || !*link || *link == '#' || *link == '?' || (link[0] == '/' && link[1] == '/')) return false;
  size_t length = 0;
  bool beforeSlash = true;
  for (const char* p = link; *p && *p != '#' && *p != '?'; ++p) {
    if (uint8_t(*p) < 32 || *p == '\\') return false;
    if (*p == '/') beforeSlash = false;
    if (*p == ':' && beforeSlash) return false; // Never follow URI schemes.
    if (++length >= capacity - 3) return false;
  }
  if (!length) return false;
  bool absolute = *link == '/';
  while (*link == '/') { ++link; --length; }
  if (length >= 5 && !std::strncmp(link, "wiki/", 5)) { link += 5; length -= 5; absolute = true; }
  bool explicitNamespace = length >= 2 && pathNamespace(link[0]) && link[1] == '/';
  size_t used = 0;
  if (!explicitNamespace) {
    if (!absolute && base && pathNamespace(base[0]) && base[1] == '/') {
      const char* slash = std::strrchr(base, '/');
      used = size_t(slash - base + 1);
      if (used >= capacity) return false;
      std::memcpy(out, base, used);
    } else { out[used++] = fallback; out[used++] = '/'; }
  }
  for (size_t i = 0; i < length; ++i) {
    uint8_t c = uint8_t(link[i]);
    if (c == '%') {
      if (i + 2 >= length) return false;
      int a = hexDigit(link[i + 1]), b = hexDigit(link[i + 2]);
      if (a < 0 || b < 0) return false;
      c = uint8_t((a << 4) | b); i += 2;
    }
    if (c < 32 || c == 127 || c == '\\') return false;
    if (used + 1 >= capacity) return false;
    out[used++] = char(c);
  }
  out[used] = 0;
  // Collapse slash and dot components in place, with no allocation.
  size_t read = 0, write = 0;
  while (read < used) {
    while (read < used && out[read] == '/') ++read;
    size_t begin = read;
    while (read < used && out[read] != '/') ++read;
    size_t n = read - begin;
    if (!n || (n == 1 && out[begin] == '.')) continue;
    if (n == 2 && out[begin] == '.' && out[begin + 1] == '.') {
      if (!write) return false;
      while (write && out[write - 1] != '/') --write;
      if (write) --write;
      continue;
    }
    if (write) out[write++] = '/';
    std::memmove(out + write, out + begin, n); write += n;
  }
  out[write] = 0;
  return write > 2 && pathNamespace(out[0]) && out[1] == '/';
}
}

bool Reader::resolveLink(const char* link, Entry& result, const char* base) {
  error_ = nullptr;
  if (!ready_ || !link) return fail("Invalid ZIM link");
  uint32_t id = UINT32_MAX;
  auto lookup = [&](const char* path) {
    if (!findPath(path[0], path + 2, id, result)) return false;
    // Some modern archives preserve the old namespace inside their C path.
    if (id == UINT32_MAX && path[0] != contentNamespace_ && !findPath(contentNamespace_, path, id, result)) return false;
    if (id == UINT32_MAX && path[0] != contentNamespace_ && !findPath(contentNamespace_, path + 2, id, result)) return false;
    return true;
  };
  // Sidecars contain decoded canonical paths. Try them literally first so a
  // percent sign, question mark or hash in an actual archive path survives a
  // second click; URL decoding below is only needed for unresolved input.
  if (pathNamespace(link[0]) && link[1] == '/' && std::strlen(link) < 1030) {
    if (!lookup(link)) return false;
    if (id != UINT32_MAX) return true;
  }
  auto* normalized = static_cast<char*>(allocate(1030));
  if (!normalized) return fail("Insufficient PSRAM for ZIM link");
  bool ok = canonicalLink(link, base, contentNamespace_, normalized, 1030);
  if (ok) ok = lookup(normalized);
  release(normalized);
  if (!ok || id == UINT32_MAX) return fail("Article or image is not available in this ZIM");
  return true;
}

class ClusterStream {
 public:
  explicit ClusterStream(Reader& owner) : reader(owner) {}
  ~ClusterStream() {
    if (decoder) ZSTD_freeDStream(decoder);
    if (xz) xz_dec_end(xz);
    reader.release(input); reader.release(output);reader.release(flat);
  }
  bool open(const Entry& item) {
    uint8_t info;
    if (!reader.clusterRange(item.cluster, cursor, end, info)) return false;
    ++cursor;begin=cursor;wantedBlob=item.blob; compression = info & 15; width = (info & 16) ? 8 : 4;
    if (compression != 0 && compression != 1 && compression != 4 && compression != 5)
      return reader.fail("Unsupported ZIM compression");
    if (compression >= 4) {
      input = static_cast<uint8_t*>(reader.allocate(InputBytes));
      output = static_cast<uint8_t*>(reader.allocate(OutputBytes));
      if (!input || !output) return reader.fail("Insufficient PSRAM for ZIM reader");
      if(compression==4){
        xz_crc32_init();xz_crc64_init();
        // XZ_SINGLE uses the actual cluster as history, not its advertised dictionary.
        size_t size=0;
        if(!xzSize(size))return false;
        if(size&&size<=FlatLimit&&end-begin<=4U*1024*1024){if(!compatibility(size))return false;}
        else {xz=xz_dec_init(XZ_DYNALLOC,8U*1024*1024);if(!xz)return reader.fail("Insufficient PSRAM for ZIM decoder");xz_dec_set_cooperate(xz,xzCooperate,&reader);}
      }
      else {
       ZSTD_frameHeader header{};uint8_t bytes[ZSTD_FRAMEHEADERSIZE_MAX];
       size_t n=size_t(std::min<uint64_t>(sizeof(bytes),end-begin));
       if(!reader.readAt(begin,bytes,n))return false;
       size_t result=ZSTD_getFrameHeader(&header,bytes,n);
       if(ZSTD_isError(result)||result)return reader.fail("Invalid ZIM compressed stream");
       if((header.frameContentSize==ZSTD_CONTENTSIZE_UNKNOWN&&header.windowSize>512U*1024)||(header.frameContentSize<=FlatLimit&&header.windowSize>512U*1024&&header.windowSize>header.frameContentSize)){
         if(!compatibility(header.frameContentSize==ZSTD_CONTENTSIZE_UNKNOWN?0:size_t(std::min<uint64_t>(header.frameContentSize,SIZE_MAX))))return false;
       } else {
       ZSTD_customMem memory = {customAlloc, customFree, this};
       decoder = ZSTD_createDStream_advanced(memory);
       if (!decoder || ZSTD_isError(ZSTD_DCtx_setParameter(decoder, ZSTD_d_windowLogMax, 23)))
        return reader.fail("Insufficient PSRAM for ZIM decoder");}
      }
    }
    uint64_t first, from, to;
    if (!number(first)) return false;
    if (first < width * 2 || first % width || first > MaxClusterScan || uint64_t(item.blob) + 1 >= first / width)
      return reader.fail("Invalid ZIM blob table");
    if (!item.blob) from = first;
    else {
      if (!skip(uint64_t(item.blob) * width - width) || !number(from)) return false;
    }
    if (!number(to)) return false;
    if (from < first || to < from || to > MaxClusterScan || to - from > MaxArticleBytes)
      return reader.fail("ZIM article or cluster is too large");
    if (from < decoded || !skip(from - decoded)) return false;
    remaining = to - from;
    return true;
  }
  size_t read(uint8_t* data, size_t n) {
    n = size_t(std::min<uint64_t>(n, remaining));
    if (!n) return 0;
    if (!bytes(data, n)) return 0;
    remaining -= n; return n;
  }
  uint64_t left() const { return remaining; }
 private:
  Reader& reader;
  uint64_t cursor = 0, begin=0, end = 0, decoded = 0, remaining = 0;
  uint8_t* flat=nullptr;size_t flatSize=0,flatAt=0;
  unsigned compression = 0, width = 4;uint32_t wantedBlob=0;
  ZSTD_DStream* decoder = nullptr;
  xz_dec* xz = nullptr;
  uint8_t* input = nullptr; uint8_t* output = nullptr;
  size_t inputSize = 0, inputAt = 0, outputSize = 0, outputAt = 0, allocated = 0;
  bool frameEnded = false;
  static int xzCooperate(void* context) { return static_cast<Reader*>(context)->cooperate(); }
  struct alignas(std::max_align_t) Allocation { size_t size; };
  static void* customAlloc(void* context, size_t n) {
    auto& self = *static_cast<ClusterStream*>(context);
    if (n > MaxDecoderBytes || self.allocated > MaxDecoderBytes - n || n > SIZE_MAX - sizeof(Allocation)) return nullptr;
    auto* allocation = static_cast<Allocation*>(self.reader.allocate(n + sizeof(Allocation)));
    if (!allocation) return nullptr;
    allocation->size = n; self.allocated += n; return allocation + 1;
  }
  static void customFree(void* context, void* pointer) {
    if (!pointer) return;
    auto& self = *static_cast<ClusterStream*>(context);
    auto* allocation = static_cast<Allocation*>(pointer) - 1;
    self.allocated -= allocation->size; self.reader.release(allocation);
  }
  static constexpr size_t FlatLimit=8U*1024*1024;
  bool xzSize(size_t& size){
    // The XZ index records the decoded size without allocating its dictionary.
    size=0;if(end-begin<24)return reader.fail("Truncated ZIM cluster");
    uint8_t footer[12];uint64_t tail=end;
    // XZ permits padding in groups of four zero bytes after the stream.
    while(tail-begin>=24){
      if(!reader.readAt(tail-12,footer,12))return false;
      if(footer[10]=='Y'&&footer[11]=='Z')break;
      if(little(footer+8,4)!=0)return reader.fail("Invalid ZIM XZ footer");
      tail-=4;
    }
    if(footer[10]!='Y'||footer[11]!='Z'||xz_crc32(footer+4,6,0)!=little(footer,4))return reader.fail("Invalid ZIM XZ footer");
    uint64_t bytes=(little(footer+4,4)+1)*4;
    if(bytes<8||bytes>tail-begin-12)return reader.fail("Invalid ZIM XZ index");
    uint64_t at=tail-12-bytes,limit=tail-16;uint32_t crc=0;uint8_t byte=0;
    auto get=[&]()->bool {if(at>=limit||!reader.readAt(at++,&byte,1))return false;crc=xz_crc32(&byte,1,crc);return true;};
    auto vli=[&](uint64_t& value)->bool {value=0;for(unsigned i=0;i<9;++i){if(!get())return false;value|=uint64_t(byte&127)<<(i*7);if(!(byte&128))return i==0||byte!=0;}return false;};
    uint64_t records=0,total=0;
    if(!get()||byte||!vli(records)||!records||records>(limit-at)/2)return reader.fail("Invalid ZIM XZ index");
    for(uint64_t i=0;i<records;++i){uint64_t packed=0,plain=0;if(!vli(packed)||!vli(plain)||!packed||plain>MaxClusterScan-total)return reader.fail("Invalid ZIM XZ index");total+=plain;}
    while(at<limit){if(!get()||byte)return reader.fail("Invalid ZIM XZ index");}
    uint8_t check[4];if(!reader.readAt(limit,check,4)||crc!=little(check,4))return reader.fail("Invalid ZIM XZ index");
    size=size_t(total);return true;
  }
  bool compatibility(size_t known=0){
    if(decoded)return reader.fail("ZIM decompression failed");
    if(decoder){ZSTD_freeDStream(decoder);decoder=nullptr;}
    if(xz){xz_dec_end(xz);xz=nullptr;}
    if(compression==4&&!known&&!xzSize(known))return false;
    if(known>FlatLimit)return reader.fail("ZIM cluster exceeds 8 MiB");
    size_t capacity=compression==5?std::min(known?known:FlatLimit,size_t(256U*1024)):(known?known:256U*1024);
    for(;;){
      flat=static_cast<uint8_t*>(reader.allocate(capacity));
      if(!flat)return reader.fail("Insufficient PSRAM for ZIM decoder");
      if(compression==4){
        if(end-begin>4U*1024*1024)return reader.fail("Legacy ZIM cluster exceeds the memory limit");
        const size_t n=size_t(end-begin);auto* encoded=static_cast<uint8_t*>(reader.allocate(n));
        if(!encoded)return reader.fail("Insufficient PSRAM for ZIM decoder");
        bool ok=reader.readAt(begin,encoded,n);xz_dec* once=xz_dec_init(XZ_SINGLE,0);
        if(ok&&once){xz_dec_set_cooperate(once,xzCooperate,&reader);xz_buf b{};b.in=encoded;b.in_size=n;b.out=flat;b.out_size=capacity;ok=xz_dec_run(once,&b)==XZ_STREAM_END;flatSize=b.out_pos;}
        else ok=false;
        xz_dec_end(once);reader.release(encoded);
        if(!ok||flatSize!=known)return reader.fail("ZIM XZ decompression failed");
        break;
      }
      ZSTD_customMem memory={customAlloc,customFree,this};decoder=ZSTD_createDStream_advanced(memory);
      if(!decoder||ZSTD_isError(ZSTD_decompressBegin(decoder)))return reader.fail("Insufficient PSRAM for ZIM decoder");
      uint64_t at=begin;size_t written=0,required=0,target=0;bool full=false,complete=false;
      // Bufferless decoding keeps history in flat. A completed block exposes the
      // ZIM offset table even when the frame omits its uncompressed size.
      while(size_t n=ZSTD_nextSrcSizeToDecompress(decoder)){
        if(n>ZSTD_BLOCKSIZE_MAX||n>end-at)return reader.fail("Invalid ZIM compressed block");
        uint8_t* encoded=input;
        if(n>InputBytes){encoded=static_cast<uint8_t*>(reader.allocate(n));if(!encoded)return reader.fail("Insufficient PSRAM for ZIM decoder");}
        bool ok=reader.readAt(at,encoded,n);size_t r=ok?ZSTD_decompressContinue(decoder,flat+written,capacity-written,encoded,n):0;
        if(encoded!=input)reader.release(encoded);if(!ok)return false;at+=n;
        if(ZSTD_isError(r)){if(ZSTD_getErrorCode(r)==ZSTD_error_dstSize_tooSmall){full=true;break;}return reader.fail("ZIM decompression failed");}
        written+=r;
        if(written>=width){uint64_t table=little(flat,width),position=(uint64_t(wantedBlob)+1)*width;
          if(table<2*width||table%width||table>MaxClusterScan||position>=table)return reader.fail("Invalid ZIM blob table");
          if(position+width<=written){uint64_t to=little(flat+position,width);
            if(to<table||to>FlatLimit)return reader.fail("ZIM article position exceeds 8 MiB");
            target=size_t(to);required=std::min(FlatLimit,target+ZSTD_BLOCKSIZE_MAX);
            if(known&&target>known)return reader.fail("Invalid ZIM blob range");
            if(written>=target){complete=true;break;}
            if(required>capacity){full=true;break;}
          }
        }
        if(!reader.cooperate())return false;
      }
      flatSize=written;ZSTD_freeDStream(decoder);decoder=nullptr;
      if(complete)break;
      if(!full){if(at!=end||(known&&flatSize!=known))return reader.fail("Invalid ZIM cluster size");break;}
      if(capacity==FlatLimit)return reader.fail("ZIM cluster exceeds 8 MiB");
      reader.release(flat);flat=nullptr;
      if(required>capacity)capacity=required;
      else capacity=std::min(FlatLimit,capacity*2);
    }
    flatAt=0;frameEnded=false;return true;
  }
  bool refill() {
    outputAt = outputSize = 0;
    if(flat){if(flatAt==flatSize)return reader.fail("Truncated ZIM cluster");outputSize=std::min(OutputBytes,flatSize-flatAt);memcpy(output,flat+flatAt,outputSize);flatAt+=outputSize;return reader.cooperate();}
    while (!outputSize) {
      if (!reader.cooperate()) return false;
      if (frameEnded) return reader.fail("Truncated ZIM cluster");
      if (inputAt == inputSize) {
        inputAt = 0; inputSize = size_t(std::min<uint64_t>(InputBytes, end - cursor));
        if (!inputSize || !reader.readAt(cursor, input, inputSize)) return reader.fail("Truncated ZIM cluster");
        cursor += inputSize;
      }
      if(xz){
       const size_t before=inputAt;xz_buf buffer{};buffer.in=input;buffer.in_pos=inputAt;buffer.in_size=inputSize;buffer.out=output;buffer.out_size=OutputBytes;
       xz_ret result=xz_dec_run(xz,&buffer);inputAt=buffer.in_pos;outputSize=buffer.out_pos;frameEnded=result==XZ_STREAM_END;
       if((result==XZ_MEMLIMIT_ERROR||result==XZ_MEM_ERROR)&&!decoded){if(!compatibility())return false;return refill();}
       if(result!=XZ_OK&&result!=XZ_STREAM_END)return reader.fail("ZIM XZ decompression failed");
       if(inputAt==before&&!outputSize&&!frameEnded)return reader.fail("Invalid ZIM compressed stream");
       continue;
      }
      ZSTD_inBuffer in = {input, inputSize, inputAt};
      ZSTD_outBuffer out = {output, OutputBytes, 0};
      const size_t before = in.pos;
      size_t result = ZSTD_decompressStream(decoder, &out, &in);
      if((ZSTD_getErrorCode(result)==ZSTD_error_frameParameter_windowTooLarge||ZSTD_getErrorCode(result)==ZSTD_error_memory_allocation)&&!decoded){if(!compatibility())return false;return refill();}
      if (ZSTD_isError(result)) return reader.fail("ZIM decompression failed or window exceeds 8 MiB");
      inputAt = in.pos; outputSize = out.pos; frameEnded = result == 0;
      if (in.pos == before && !out.pos && !frameEnded) return reader.fail("Invalid ZIM compressed stream");
    }
    return true;
  }
  bool bytes(uint8_t* data, size_t n) {
    if (decoded > MaxClusterScan || n > MaxClusterScan - decoded) return reader.fail("ZIM cluster is too large");
    if (compression < 2) {
      if (!range(cursor, n, end) || !reader.readAt(cursor, data, n)) return reader.fail("Truncated ZIM cluster");
      cursor += n; decoded += n; return true;
    }
    while (n) {
      if (outputAt == outputSize && !refill()) return false;
      size_t take = std::min(n, outputSize - outputAt);
      std::memcpy(data, output + outputAt, take);
      outputAt += take; decoded += take; data += take; n -= take;
    }
    return true;
  }
  bool number(uint64_t& value) {
    uint8_t data[8];
    if (!bytes(data, width)) return false;
    value = little(data, width); return true;
  }
  bool skip(uint64_t n) {
    if (n > MaxClusterScan - decoded) return reader.fail("ZIM cluster is too large");
    if (compression < 2) {
      if (!range(cursor, n, end)) return reader.fail("Truncated ZIM cluster");
      cursor += n; decoded += n; return reader.cooperate();
    }
    while (n) {
      if (outputAt == outputSize && !refill()) return false;
      size_t take = size_t(std::min<uint64_t>(n, outputSize - outputAt));
      outputAt += take; decoded += take; n -= take;
    }
    return true;
  }
};

namespace {
class PlainText {
 public:
  PlainText(Sink sink, bool html, const Entry& item, const RichOptions* rich)
      : sink_(sink), html_(html), rich_(rich), table_([](void* context,const char* data,size_t size){auto* self=static_cast<PlainText*>(context);return !self->rich_||!self->rich_->links.write||self->rich_->links.write(self->rich_->links.context,reinterpret_cast<const uint8_t*>(data),size);},this) {
    std::snprintf(base_, sizeof(base_), "%c/%s", item.nameSpace, item.path);
  }
  bool feed(const uint8_t* data, size_t n) {
    for (size_t i = 0; i < n && good_; ++i) consume(data[i]);
    return good_;
  }
  bool finish() {
    if (inEntity_) { emit('&'); for (unsigned i = 0; i < entitySize_; ++i) emit(entity_[i]); }
    closeAnchor();table_.finish(uint32_t(position()));good_=good_&&table_.good();newline(1); return flush();
  }
  uint64_t written() const { return written_; }
 private:
  Sink sink_; bool html_, good_ = true, inTag_ = false, quote_ = false, space_ = false, inEntity_ = false;
  uint8_t quoteCharacter_ = 0; unsigned tagSize_ = 0, entitySize_ = 0, newlines_ = 2;
  char tag_[4096] = {}, entity_[32] = {}, suppressed_[16] = {};
  const RichOptions* rich_;
  Tab5WikiTable::Recorder table_;
  char base_[1030] = {}, attribute_[1030] = {}, target_[1030] = {}, anchor_[1030] = {};
  uint64_t anchorStart_ = UINT64_MAX, anchorEnd_ = 0;
  unsigned imageCount_ = 0, linkCount_ = 0, commentTail_ = 0;
  bool tagOverflow_ = false, inComment_ = false;
  uint8_t pending_[1024]; size_t used_ = 0; uint64_t written_ = 0;
  uint64_t position() const { return written_ + used_; }
  void closeAnchor() {
    if (anchor_[0] && anchorStart_ != UINT64_MAX && anchorEnd_ > anchorStart_ && rich_ && rich_->links.write && linkCount_ < 2048) {
      char offsets[48];
      int n = std::snprintf(offsets, sizeof(offsets), "%llu\t%llu\t", (unsigned long long)anchorStart_, (unsigned long long)anchorEnd_);
      Sink sink = rich_->links;
      good_ = good_ && sink.write(sink.context, reinterpret_cast<const uint8_t*>(offsets), size_t(n)) &&
        sink.write(sink.context, reinterpret_cast<const uint8_t*>(anchor_), std::strlen(anchor_)) &&
        sink.write(sink.context, reinterpret_cast<const uint8_t*>("\n"), 1);
      ++linkCount_;
    }
    anchor_[0] = 0; anchorStart_ = UINT64_MAX; anchorEnd_ = 0;
  }
  static bool attributeName(const char* p, size_t n, const char* wanted) {
    if (n != std::strlen(wanted)) return false;
    for (size_t i = 0; i < n; ++i) {
      char c = p[i]; if (c >= 'A' && c <= 'Z') c += 32;
      if (c != wanted[i]) return false;
    }
    return true;
  }
  bool attribute(const char* wanted) {
    const char* p = tag_;
    while (*p && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') ++p;
    while (*p) {
      while (*p && uint8_t(*p) <= 32) ++p;
      const char* name = p;
      while (*p && uint8_t(*p) > 32 && *p != '=' && *p != '/') ++p;
      size_t length = size_t(p - name);
      while (*p && uint8_t(*p) <= 32) ++p;
      if (*p != '=') { if (*p) ++p; continue; }
      ++p; while (*p && uint8_t(*p) <= 32) ++p;
      char quote = (*p == '\'' || *p == '"') ? *p++ : 0;
      const char* begin = p;
      while (*p && (quote ? *p != quote : uint8_t(*p) > 32)) ++p;
      size_t n = size_t(p - begin); if (quote && *p) ++p;
      if (!attributeName(name, length, wanted)) continue;
      if (!n || n >= sizeof(attribute_)) return false;
      // HTML URL attributes commonly encode ampersands and quotes. Numeric
      // character references are decoded as UTF-8 before percent decoding.
      size_t out = 0;
      for (size_t i = 0; i < n; ++i) {
        uint32_t c = uint8_t(begin[i]);
        if (c == '&') {
          size_t end = i + 1; while (end < n && end - i < 24 && begin[end] != ';') ++end;
          if (end < n && begin[end] == ';') {
            char entity[24]; size_t count = end - i - 1;
            std::memcpy(entity, begin + i + 1, count); entity[count] = 0;
            uint32_t decoded = 0;
            if (!std::strcmp(entity, "amp")) decoded = '&';
            else if (!std::strcmp(entity, "quot")) decoded = '"';
            else if (!std::strcmp(entity, "apos")) decoded = '\'';
            else if (!std::strcmp(entity, "lt")) decoded = '<';
            else if (!std::strcmp(entity, "gt")) decoded = '>';
            else if (*entity == '#') {
              bool hex = entity[1] == 'x' || entity[1] == 'X'; char* tail = nullptr;
              unsigned long value = std::strtoul(entity + (hex ? 2 : 1), &tail, hex ? 16 : 10);
              if (tail && !*tail && value && value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff)) decoded = uint32_t(value);
            }
            if (decoded) { c = decoded; i = end; }
          }
        }
        if (out + 4 >= sizeof(attribute_)) return false;
        if (c < 128) attribute_[out++] = char(c);
        else if (c < 256 && c == uint8_t(begin[i])) attribute_[out++] = char(c);
        else if (c < 2048) { attribute_[out++] = char(0xc0 | (c >> 6)); attribute_[out++] = char(0x80 | (c & 63)); }
        else if (c < 65536) { attribute_[out++] = char(0xe0 | (c >> 12)); attribute_[out++] = char(0x80 | ((c >> 6) & 63)); attribute_[out++] = char(0x80 | (c & 63)); }
        else { attribute_[out++] = char(0xf0 | (c >> 18)); attribute_[out++] = char(0x80 | ((c >> 12) & 63)); attribute_[out++] = char(0x80 | ((c >> 6) & 63)); attribute_[out++] = char(0x80 | (c & 63)); }
      }
      attribute_[out] = 0; return true;
    }
    return false;
  }
  bool flush() {
    if (!good_) return false;
    if (used_) {
      good_ = sink_.write && sink_.write(sink_.context, pending_, used_);
      if (good_) { written_ += used_; used_ = 0; }
    }
    return good_;
  }
  void raw(uint8_t c) {
    if (!good_) return;
    if (used_ == sizeof(pending_) && !flush()) return;
    pending_[used_++] = c;
  }
  void emit(uint8_t c) {
    if (c <= 32 || c == 127) { if (c == '\n' && !html_) newline(1); else space_ = true; return; }
    if (space_ && !newlines_) raw(' ');
    space_ = false; newlines_ = 0;
    if (anchor_[0] && anchorStart_ == UINT64_MAX) anchorStart_ = position();
    raw(c);
    if (anchor_[0]) anchorEnd_ = position();
  }
  void newline(unsigned count) {
    space_ = false;
    while (newlines_ < count) { raw('\n'); ++newlines_; }
  }
  void codepoint(uint32_t c) {
    if (c == 160) { space_ = true; return; }
    if (!c || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) c = 0xfffd;
    if (c < 128) emit(uint8_t(c));
    else if (c < 2048) { emit(uint8_t(0xc0 | (c >> 6))); emit(uint8_t(0x80 | (c & 63))); }
    else if (c < 65536) { emit(uint8_t(0xe0 | (c >> 12))); emit(uint8_t(0x80 | ((c >> 6) & 63))); emit(uint8_t(0x80 | (c & 63))); }
    else { emit(uint8_t(0xf0 | (c >> 18))); emit(uint8_t(0x80 | ((c >> 12) & 63))); emit(uint8_t(0x80 | ((c >> 6) & 63))); emit(uint8_t(0x80 | (c & 63))); }
  }
  void entity() {
    entity_[entitySize_] = 0;
    uint32_t c = 0;
    if (entity_[0] == '#') {
      char* end = nullptr; bool hex = entity_[1] == 'x' || entity_[1] == 'X';
      unsigned long value = std::strtoul(entity_ + (hex ? 2 : 1), &end, hex ? 16 : 10);
      if (end && !*end && value <= 0x10ffff) c = uint32_t(value);
    } else {
      struct Entity { const char* name; uint32_t value; };
      static const Entity entities[] = {{"amp",38},{"lt",60},{"gt",62},{"quot",34},{"apos",39},{"nbsp",160},{"ndash",8211},{"mdash",8212},{"hellip",8230},{"laquo",171},{"raquo",187},{"copy",169},{"reg",174},{"deg",176},{"times",215},{"minus",8722},{"middot",183},{"eacute",233},{"Eacute",201},{"agrave",224},{"ecirc",234},{"egrave",232},{"ccedil",231}};
      for (const auto& e : entities) if (!std::strcmp(entity_, e.name)) { c = e.value; break; }
    }
    if (c) codepoint(c);
    else { emit('&'); for (unsigned i = 0; i < entitySize_; ++i) emit(entity_[i]); emit(';'); }
    entitySize_ = 0; inEntity_ = false;
  }
  void tag() {
    if (tagOverflow_) return;
    tag_[tagSize_] = 0; char name[16] = {}; unsigned i = 0, j = 0;
    while (tag_[i] == ' ' || tag_[i] == '\n') ++i;
    bool closing = tag_[i] == '/'; if (closing) ++i;
    while (tag_[i] && j + 1 < sizeof(name)) {
      unsigned char c = uint8_t(tag_[i++]);
      if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')) break;
      name[j++] = char(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
    if (suppressed_[0]) {
      if (closing && !std::strcmp(name, suppressed_)) suppressed_[0] = 0;
      return;
    }
    if (!closing && (!std::strcmp(name,"head") || !std::strcmp(name,"script") || !std::strcmp(name,"style") || !std::strcmp(name,"svg"))) {
      std::strcpy(suppressed_, name); return;
    }
    if (rich_ && !std::strcmp(name,"a")) {
      closeAnchor();
      if (!closing && rich_->links.write && attribute("href") && canonicalLink(attribute_, base_, base_[0], target_, sizeof(target_)))
        std::strcpy(anchor_, target_);
    }
    if (rich_ && !closing && !std::strcmp(name,"img") && rich_->image && imageCount_ < std::min(rich_->maxImages, 8u) &&
        ((attribute("src") && canonicalLink(attribute_, base_, base_[0], target_, sizeof(target_))) ||
         (attribute("data-src") && canonicalLink(attribute_, base_, base_[0], target_, sizeof(target_))))) {
      newline(1);
      good_ = good_ && rich_->image(rich_->context, position(), target_); ++imageCount_;
    }
    if(!std::strcmp(name,"table")){
      closeAnchor();if(closing)table_.end(uint32_t(position()));else{newline(2);table_.begin(uint32_t(position()));}
    }
    if(!std::strcmp(name,"tr")&&table_.depth()==1){closeAnchor();table_.closeCell(uint32_t(position()));newline(1);if(!closing)table_.row(uint32_t(position()));}
    if((!std::strcmp(name,"td")||!std::strcmp(name,"th"))&&table_.depth()==1){
      closeAnchor();table_.closeCell(uint32_t(position()));
      if(!closing){unsigned colspan=attribute("colspan")?std::max(1,std::atoi(attribute_)):1;unsigned rowspan=attribute("rowspan")?std::max(1,std::atoi(attribute_)):1;
        if(!newlines_){raw(' ');space_=false;}table_.openCell(uint32_t(position()),!std::strcmp(name,"th"),colspan,rowspan);}
    }
    good_=good_&&table_.good();
    if (!std::strcmp(name,"p") || !std::strcmp(name,"section") || !std::strcmp(name,"article") ||
        !std::strcmp(name,"div") || (name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && !name[2])) newline(2);
    else if (!std::strcmp(name,"br") || !std::strcmp(name,"tr") || !std::strcmp(name,"li") || !std::strcmp(name,"dt") || !std::strcmp(name,"dd")) newline(1);
    else if (!std::strcmp(name,"td") || !std::strcmp(name,"th")) space_ = true;
    if (!closing && !std::strcmp(name,"li")) { emit('-'); space_ = true; }
  }
  void consume(uint8_t c) {
    if (!html_) { emit(c); return; }
    if (inComment_) {
      if (c == '-') commentTail_ = std::min(commentTail_ + 1, 2u);
      else { if (c == '>' && commentTail_ == 2) inComment_ = false; commentTail_ = 0; }
      return;
    }
    if (inTag_) {
      if (tagSize_ == 3 && !std::memcmp(tag_, "!--", 3)) {
        inTag_ = false; inComment_ = true; commentTail_ = c == '-' ? 1 : 0; return;
      }
      if ((c == '\'' || c == '"') && (!quote_ || c == quoteCharacter_)) { quote_ = !quote_; quoteCharacter_ = c; }
      if (c == '>' && !quote_) { inTag_ = false; tag(); tagSize_ = 0; }
      else if (tagSize_ + 1 < sizeof(tag_)) tag_[tagSize_++] = char(c);
      else tagOverflow_ = true;
      return;
    }
    if (c == '<') {
      if (inEntity_) { emit('&'); for (unsigned i = 0; i < entitySize_; ++i) emit(entity_[i]); entitySize_ = 0; inEntity_ = false; }
      inTag_ = true; tagSize_ = 0; quote_ = false; tagOverflow_ = false; return;
    }
    if (suppressed_[0]) return;
    if (inEntity_) {
      if (c == ';') { entity(); return; }
      if (c > 32 && c < 127 && c != '&' && entitySize_ + 1 < sizeof(entity_)) { entity_[entitySize_++] = char(c); return; }
      emit('&'); for (unsigned i = 0; i < entitySize_; ++i) emit(entity_[i]); entitySize_ = 0; inEntity_ = false;
    }
    if (c == '&') { entitySize_ = 0; inEntity_ = true; return; }
    emit(c);
  }
};
}

bool Reader::extractText(uint32_t id, Sink sink, Entry* resolved, uint64_t* bytesWritten, const RichOptions* rich) {
  error_ = nullptr;
  if (bytesWritten) *bytesWritten = 0;
  if (!ready_ || !sink.write) return fail("Invalid ZIM extraction");
  Entry item;
  unsigned redirects = 0;
  do {
    if (++redirects > 32) return fail("ZIM redirect loop");
    if (!entry(id, item)) return false;
    id = item.redirect;
  } while (id != UINT32_MAX);
  char mime[192];
  if (item.mime >= 0xfffd || !mimeType(item.mime, mime, sizeof(mime))) return fail("Unsupported ZIM entry");
  const bool html = !std::strncmp(mime, "text/html", 9) || !std::strncmp(mime, "application/xhtml+xml", 21);
  if (!html && std::strncmp(mime, "text/plain", 10)) return fail("This ZIM entry is not readable text");
  ClusterStream stream(*this);
  if (!stream.open(item)) return false;
  auto* input = static_cast<uint8_t*>(allocate(OutputBytes));
  auto* parserMemory = allocate(sizeof(PlainText));
  if (!input || !parserMemory) { release(input); release(parserMemory); return fail("Insufficient PSRAM for ZIM article"); }
  PlainText* parser = new (parserMemory) PlainText(sink, html, item, rich);
  bool ok = true;
  while (stream.left()) {
    if (!cooperate()) { ok = false; break; }
    const size_t n = stream.read(input, OutputBytes);
    if (!n || !parser->feed(input, n)) { ok = false; break; }
  }
  if (ok) ok = parser->finish();
  if (ok) {
    if (resolved) *resolved = item;
    if (bytesWritten) *bytesWritten = parser->written();
  }
  parser->~PlainText(); release(parserMemory); release(input);
  return ok ? true : fail("Cannot write ZIM article");
}

bool Reader::extractImage(uint32_t id, Sink sink, uint64_t* bytesWritten, uint64_t limit) {
  error_ = nullptr;
  if (bytesWritten) *bytesWritten = 0;
  if (!ready_ || !sink.write || !limit || limit > 131072) return fail("Invalid ZIM image extraction");
  Entry item; unsigned redirects = 0;
  do {
    if (++redirects > 32) return fail("ZIM redirect loop");
    if (!entry(id, item)) return false;
    id = item.redirect;
  } while (id != UINT32_MAX);
  char mime[192];
  if (item.mime >= 0xfffd || !mimeType(item.mime, mime, sizeof(mime))) return fail("Unsupported ZIM image");
  if (std::strcmp(mime, "image/jpeg") && std::strcmp(mime, "image/png") && std::strcmp(mime, "image/webp"))
    return fail("Unsupported ZIM image format");
  ClusterStream stream(*this);
  if (!stream.open(item)) return false;
  if (stream.left() > limit) return fail("ZIM image is too large");
  auto* input = static_cast<uint8_t*>(allocate(OutputBytes));
  if (!input) return fail("Insufficient PSRAM for ZIM image");
  uint64_t written = 0; bool ok = true;
  while (stream.left()) {
    if (!cooperate()) { ok = false; break; }
    size_t n = stream.read(input, OutputBytes);
    if (!n || !sink.write(sink.context, input, n)) { ok = false; break; }
    written += n;
  }
  release(input);
  if (ok && bytesWritten) *bytesWritten = written;
  return ok ? true : fail("Cannot write ZIM image");
}

#ifdef ARDUINO
struct SdSource::State {
  Tab5StorageUse lease;
  fs::FS* fs = nullptr;
  fs::File file;
  String path;
  uint64_t size = 0;
  uint32_t currentPart = UINT32_MAX;
  bool split = false, kiwix = false;
  uint64_t* ends=nullptr;unsigned parts=0;
  ~State(){std::free(ends);}
};
SdSource::SdSource() = default;
SdSource::~SdSource() { close(); }
void SdSource::close() { delete state_; state_ = nullptr; }
bool SdSource::open(fs::FS& fs, const String& path, String& error) {
  close(); error = "";
  state_ = new (std::nothrow) State;
  if (!state_) { error = "Insufficient PSRAM for ZIM archive"; return false; }
  if (!state_->lease) { error = "SD card is unavailable"; close(); return false; }
  state_->fs = &fs; state_->path = path;
  state_->file = fs.open(path, FILE_READ);
  if (!state_->file) { error = "Cannot open ZIM archive"; close(); return false; }
  if(path.endsWith(".zimaa")){
    state_->file.close();state_->kiwix=true;state_->path=path.substring(0,path.length()-2);
#ifdef ARDUINO_ARCH_ESP32
    state_->ends=static_cast<uint64_t*>(heap_caps_calloc(676,sizeof(uint64_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
    state_->ends=static_cast<uint64_t*>(std::calloc(676,sizeof(uint64_t)));
#endif
    if(!state_->ends){error="Insufficient PSRAM for ZIM archive";close();return false;}
    for(unsigned i=0;i<676;++i){char suffix[3]={char('a'+i/26),char('a'+i%26),0};File part=fs.open(state_->path+suffix,FILE_READ);if(!part)break;
     if(part.isDirectory()||!part.size()){error="Incomplete or invalid ZIM archive";close();return false;}
     state_->size+=part.size();state_->ends[state_->parts++]=state_->size;part.close();delay(1);
    }
    if(!state_->parts){error="Cannot open ZIM archive";close();return false;}
    return true;
  }
  state_->split = state_->file.isDirectory();
  if (state_->split) {
    state_->file.close();
    fs::File manifest = fs.open(path + "/manifest.json", FILE_READ);
    if (!manifest || manifest.size() > 8192) { error = "Invalid ZIM manifest"; close(); return false; }
    JsonDocument doc;
    DeserializationError parse = deserializeJson(doc, manifest);
    manifest.close();
    if (parse || String(doc["format"] | "") != "tab5-zim-parts-v1" || !doc["complete"].is<bool>() || !doc["complete"].as<bool>() ||
        !doc["size"].is<uint64_t>() || doc["part_size"].as<uint64_t>() != PartBytes) {
      error = "ZIM download is incomplete or manifest is invalid"; close(); return false;
    }
    state_->size = doc["size"].as<uint64_t>();
    if (state_->size < 96 || (state_->size - 1) / PartBytes > 999999) {
      error = "Invalid ZIM archive size"; close(); return false;
    }
  } else state_->size = state_->file.size();
  return true;
}
Source SdSource::source() const {
  Source result;
  if (state_) { result.context = state_; result.size = state_->size; result.readAt = read; }
  return result;
}
bool SdSource::read(void* context, uint64_t at, uint8_t* data, size_t n) {
  auto& state = *static_cast<State*>(context);
  if (!range(at, n, state.size)) return false;
  while (n) {
    uint32_t part = state.split ? uint32_t(at / PartBytes) : 0;
    uint64_t offset = state.split ? at % PartBytes : at;
    size_t take = state.split ? size_t(std::min<uint64_t>(n, PartBytes - offset)) : n;
    if(state.kiwix){part=0;while(part<state.parts&&at>=state.ends[part])++part;if(part>=state.parts)return false;
     const uint64_t begin=part?state.ends[part-1]:0;offset=at-begin;take=size_t(std::min<uint64_t>(n,state.ends[part]-at));
     if(state.currentPart!=part){state.file.close();char suffix[3]={char('a'+part/26),char('a'+part%26),0};state.file=state.fs->open(state.path+suffix,FILE_READ);
      if(!state.file||state.file.size()!=state.ends[part]-begin)return false;state.currentPart=part;}
    }
    if (offset > UINT32_MAX) return false;
    if (state.split && state.currentPart != part) {
      state.file.close();
      char suffix[32]; std::snprintf(suffix, sizeof(suffix), "/part-%06lu.bin", static_cast<unsigned long>(part));
      state.file = state.fs->open(state.path + suffix, FILE_READ);
      if (!state.file) return false;
      uint64_t expected = std::min<uint64_t>(PartBytes, state.size - uint64_t(part) * PartBytes);
      if (state.file.size() != expected) { state.file.close(); return false; }
      state.currentPart = part;
    }
    if (!state.file || !state.file.seek(uint32_t(offset), SeekSet)) return false;
    if (state.file.read(data, take) != take) return false;
    data += take; at += take; n -= take;
  }
  return true;
}
#endif
} // namespace Tab5Zim
