#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
class Preferences {
 public:
  inline static std::map<std::string,std::vector<unsigned char>> values;
  inline static bool available=true,writable=true;
  inline static unsigned writes=0,attempts=0;
  bool begin(const char*,bool=false){return available;}
  void end(){}
  size_t getBytesLength(const char* key){auto i=values.find(key);return i==values.end()?0:i->second.size();}
  size_t getBytes(const char* key,void* out,size_t n){auto i=values.find(key);if(i==values.end()||n<i->second.size())return 0;std::memcpy(out,i->second.data(),i->second.size());return i->second.size();}
  size_t putBytes(const char* key,const void* in,size_t n){++attempts;if(!writable)return 0;++writes;auto* bytes=static_cast<const unsigned char*>(in);values[key]={bytes,bytes+n};return n;}
};
