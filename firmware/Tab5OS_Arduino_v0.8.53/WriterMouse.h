#pragma once
#include <cstddef>
#include <cstdint>
namespace writing {
struct MouseReport { int16_t x=0,y=0,wheel=0,horizontal=0;uint8_t buttons=0,buttonMask=0; };
class HidMouseDescriptor {
 public:
 bool parse(const uint8_t*,size_t);
 bool decode(const uint8_t*,size_t,MouseReport&)const;
 static bool boot(const uint8_t*,size_t,MouseReport&);
 private:
 struct Field {uint16_t offset;uint8_t size,id,usage;bool sign;};
 Field fields_[32]{};uint16_t lengths_[256]{};uint8_t count_=0;bool ids_=false;
};
}
