#pragma once
#include <cstdlib>
#include <cstring>
#include <map>
#include <cstdio>
#define MALLOC_CAP_SPIRAM 1u
#define MALLOC_CAP_8BIT 2u
namespace HeapCheck {inline std::map<void*,size_t> allocations;inline size_t live=0,peak=0,calls=0;inline int failAfter=-1;}
inline void*heap_caps_realloc(void*old,size_t n,unsigned caps){
 ++HeapCheck::calls;
 if(caps!=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))std::abort();
 if(HeapCheck::failAfter==0)return nullptr;if(HeapCheck::failAfter>0)--HeapCheck::failAfter;
 const auto it=HeapCheck::allocations.find(old);if(old&&it==HeapCheck::allocations.end())std::abort();
 const size_t previous=old?it->second:0;
 void*p=std::realloc(old,n);if(!p)return nullptr;
 if(old)HeapCheck::allocations.erase(old);HeapCheck::allocations[p]=n;HeapCheck::live+=n;HeapCheck::live-=previous;
 if(HeapCheck::peak<HeapCheck::live)HeapCheck::peak=HeapCheck::live;return p;
}
inline void*heap_caps_malloc(size_t n,unsigned caps){return heap_caps_realloc(nullptr,n,caps);}
inline void*heap_caps_calloc(size_t n,size_t size,unsigned caps){void*p=heap_caps_malloc(n*size,caps);if(p)std::memset(p,0,n*size);return p;}
inline void heap_caps_free(void*p){if(!p)return;const auto it=HeapCheck::allocations.find(p);if(it==HeapCheck::allocations.end())std::abort();HeapCheck::live-=it->second;HeapCheck::allocations.erase(it);std::free(p);}
