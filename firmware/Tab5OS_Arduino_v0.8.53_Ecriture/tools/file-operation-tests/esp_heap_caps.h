#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
inline void* heap_caps_calloc(size_t n, size_t size, int) { return calloc(n,size); }
inline void* heap_caps_malloc(size_t size, int) { return malloc(size); }
