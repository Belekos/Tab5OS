#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
class Preferences {
 public:
 inline static std::map<std::string,std::vector<unsigned char>> values;
 inline static bool available=true,writable=true;
 bool begin(const char*,bool=false){return available;}
 void end(){}
 bool isKey(const char* key){return values.count(key)!=0;}
 size_t getBytesLength(const char* key){return isKey(key)?values[key].size():0;}
 size_t getBytes(const char* key,void* out,size_t size){if(!isKey(key)||size<values[key].size())return 0;memcpy(out,values[key].data(),values[key].size());return values[key].size();}
 size_t putBytes(const char* key,const void* input,size_t size){if(!writable)return 0;const auto* bytes=static_cast<const unsigned char*>(input);values[key]={bytes,bytes+size};return size;}
 bool remove(const char* key){return writable&&values.erase(key)!=0;}
};
