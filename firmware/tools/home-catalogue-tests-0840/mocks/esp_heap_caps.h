#pragma once
#include <cstdlib>
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
namespace MockHeap {inline bool fail=false;inline unsigned allocations=0;}
inline void* heap_caps_malloc(size_t n,unsigned){++MockHeap::allocations;return MockHeap::fail?nullptr:malloc(n);}
inline void heap_caps_free(void* p){free(p);}
