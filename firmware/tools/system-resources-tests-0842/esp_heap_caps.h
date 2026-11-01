#pragma once
#include <cstdlib>
#include <map>
#include <cassert>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
namespace memory { inline int failAt=0, calls=0; inline size_t peak=0, live=0; inline std::map<void*,size_t> blocks; }
inline void* heap_caps_malloc(size_t n,int caps) {assert(caps==3); if(++memory::calls==memory::failAt)return nullptr;void* p=malloc(n);if(p){memory::blocks[p]=n;memory::live+=n;memory::peak=std::max(memory::peak,memory::live);}return p;}
inline void heap_caps_free(void* p){if(!p)return;assert(memory::blocks.count(p));memory::live-=memory::blocks[p];memory::blocks.erase(p);free(p);}
