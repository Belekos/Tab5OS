#pragma once
#include <vector>
#include <cstring>
class Preferences{public:
 inline static std::vector<unsigned char> bytes;
 bool begin(const char*,bool){return true;} void end(){}
 size_t getBytesLength(const char*){return bytes.size();}
 size_t getBytes(const char*,void* p,size_t n){if(n!=bytes.size())return 0;memcpy(p,bytes.data(),n);return n;}
 size_t putBytes(const char*,const void* p,size_t n){bytes.assign((const unsigned char*)p,(const unsigned char*)p+n);return n;}
};
