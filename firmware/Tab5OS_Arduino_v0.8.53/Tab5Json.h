#pragma once
#ifndef ARDUINOJSON_ENABLE_STD_STREAM
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#endif
#ifndef ARDUINOJSON_ENABLE_STD_STRING
#define ARDUINOJSON_ENABLE_STD_STRING 0
#endif
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

// Parsing catalogs must not compete with TLS for the small internal heap.
// ArduinoJson propagates allocation failure as NoMemory; there is no fallback.
class Tab5JsonAllocator final : public ArduinoJson::Allocator {
 public:
  void* allocate(size_t bytes) override {
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  void deallocate(void* data) override { heap_caps_free(data); }
  void* reallocate(void* data, size_t bytes) override {
    return heap_caps_realloc(data, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
};
inline ArduinoJson::Allocator* tab5JsonAllocator() {
  static Tab5JsonAllocator allocator;
  return &allocator;
}
