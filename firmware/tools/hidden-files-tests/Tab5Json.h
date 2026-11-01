#pragma once
#include <ArduinoJson.h>
#include <cstdlib>
class HostAllocator:public ArduinoJson::Allocator {
 public:
 void* allocate(size_t n)override{return malloc(n);}
 void deallocate(void* p)override{free(p);}
 void* reallocate(void* p,size_t n)override{return realloc(p,n);}
};
inline ArduinoJson::Allocator* tab5JsonAllocator(){static HostAllocator a;return &a;}
