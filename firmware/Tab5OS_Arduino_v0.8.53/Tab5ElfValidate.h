#pragma once
#include <stddef.h>
#include <stdint.h>
namespace Tab5ElfValidate {
constexpr size_t MaxFile=4*1024*1024, MaxImage=8*1024*1024;
struct Image { uint32_t base=0,bytes=0,entry=0,stack=16384; };
// Pure bounded parser, usable in host QA. Optional callback yields/cancels.
bool validate(const uint8_t* file,size_t length,Image& image,
              const char** reason=nullptr,bool (*cooperate)()=nullptr);
}
