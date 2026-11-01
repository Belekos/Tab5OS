#pragma once
#include <cstdlib>
#include <map>
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
namespace MockHeap {inline bool fail=false;inline unsigned allocations=0;inline std::map<void*,size_t> live;inline size_t bytes(){size_t n=0;for(auto&v:live)n+=v.second;return n;}}
inline void* heap_caps_malloc(size_t n,unsigned){++MockHeap::allocations;void*p=MockHeap::fail?nullptr:malloc(n);if(p)MockHeap::live[p]=n;return p;}
inline void heap_caps_free(void*p){MockHeap::live.erase(p);free(p);}
