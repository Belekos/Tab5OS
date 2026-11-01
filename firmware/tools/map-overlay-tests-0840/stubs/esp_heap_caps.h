#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
extern bool failAllocation;extern size_t allocatedBytes,allocationCalls;
inline void* heap_caps_calloc(size_t n,size_t s,unsigned flags){if(flags!=3)std::abort();++allocationCalls;if(failAllocation)return nullptr;void*p=std::calloc(n,s);if(p)allocatedBytes=n*s;return p;}
inline void heap_caps_free(void*p){if(p)allocatedBytes=0;std::free(p);}
