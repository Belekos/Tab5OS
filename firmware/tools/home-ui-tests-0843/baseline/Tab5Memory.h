#pragma once
#include <stddef.h>
#include <stdint.h>
#include <esp_heap_caps.h>
#include "Tab5ResourcePolicy.h"
#include <type_traits>

// Trivial tables grow only as entries are needed. No hidden internal-RAM
// fallback and no allocation in a static constructor.
template<class T, size_t Limit> class Tab5PsramTable {
 public:
  static_assert(std::is_trivially_copyable<T>::value, "table entries must be trivial");
  ~Tab5PsramTable() { clear(); }
  Tab5PsramTable() = default;
  Tab5PsramTable(const Tab5PsramTable&) = delete;
  Tab5PsramTable& operator=(const Tab5PsramTable&) = delete;
  bool ensure(size_t entries) {
    if (entries > Limit) return false;
    if (entries <= _capacity) return true;
    size_t next = _capacity ? _capacity : 64;
    if (next > Limit) next = Limit;
    while (next < entries) next = next > Limit / 2 ? Limit : next * 2;
    T* data = static_cast<T*>(heap_caps_realloc(_data, next * sizeof(T),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!data) return false;
    _data = data; _capacity = next; return true;
  }
  void clear() { heap_caps_free(_data); _data = nullptr; _capacity = 0; }
  T& operator[](size_t index) { return _data[index]; }
  const T& operator[](size_t index) const { return _data[index]; }
  size_t capacity() const { return _capacity; }
 private:
  T* _data = nullptr;
  size_t _capacity = 0;
};

// Sole owner of a variable-sized byte allocation in PSRAM. A failed realloc
// never loses the previous allocation. No fallback into scarce internal RAM.
class Tab5ByteBuffer {
 public:
  explicit Tab5ByteBuffer(size_t limit, size_t initial = 0) : _limit(limit) {
    if (initial) reserve(initial);
  }
  ~Tab5ByteBuffer() { heap_caps_free(_data); }
  Tab5ByteBuffer(const Tab5ByteBuffer&) = delete;
  Tab5ByteBuffer& operator=(const Tab5ByteBuffer&) = delete;
  bool reserve(size_t bytes);
  bool append(const uint8_t* source, size_t length);
  uint8_t* data() const { return _data; }
  size_t size() const { return _size; }
  size_t capacity() const { return _capacity; }
  bool exhausted() const { return _exhausted; }
  bool overflowed() const { return _overflowed; }
  uint8_t* detach();
 private:
  uint8_t* _data = nullptr;
  size_t _limit = 0, _size = 0, _capacity = 0;
  bool _exhausted = false, _overflowed = false;
  bool _detached = false;
};

void tab5LogMemory(const char* stage);
Tab5Resources::MemorySample tab5MemorySample();
namespace Tab5Resources { struct Budget; }
// Startup admission: only the explicitly requested contiguous blocks are read.
Tab5Resources::MemorySample tab5MemorySample(const Tab5Resources::Budget& requested);
Tab5Resources::MemoryTotals tab5MemoryTotals();
// Explicit diagnostic only; it walks stacks once, never in the periodic UI path.
// The callback receives bounded lines. No TaskStatus_t name/handle is retained.
bool tab5DumpTasks(void (*emit)(const char*,size_t,void*),void* context);
// Explicit diagnostic only: detailed capability heaps including low-water marks.
bool tab5DumpMemory(void (*emit)(const char*,size_t,void*),void* context);
void tab5TaskDiagnosticsBegin();
void tab5TaskStarted(const char* name);
