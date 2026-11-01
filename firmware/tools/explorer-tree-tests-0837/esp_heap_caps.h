#pragma once
#include <cstdlib>
#include <cassert>
#include <map>
inline constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
inline std::map<void*,size_t> allocations;
inline long failAfter=-1;
inline size_t liveBytes=0,peakBytes=0;
inline bool refuse(){if(failAfter<0)return false;if(!failAfter)return true;--failAfter;return false;}
inline void* heap_caps_realloc(void* old,size_t bytes,unsigned caps){
  assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(refuse())return nullptr;
  size_t previous=old?allocations.at(old):0;
  void* p=std::realloc(old,bytes);if(!p)return nullptr;
  if(old)allocations.erase(old);allocations[p]=bytes;liveBytes+=bytes-previous;if(liveBytes>peakBytes)peakBytes=liveBytes;return p;
}
inline void* heap_caps_malloc(size_t bytes,unsigned caps){return heap_caps_realloc(nullptr,bytes,caps);}
inline void heap_caps_free(void* p){if(!p)return;liveBytes-=allocations.at(p);allocations.erase(p);std::free(p);}
