#pragma once
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
namespace preferences_test {
inline std::map<std::string, std::vector<unsigned char>> blobs;
inline unsigned writes=0,reads=0,begins=0,ends=0;
inline bool failReadOpen=false,failWriteOpen=false,shortRead=false,shortWrite=false;
inline void reset(){blobs.clear();writes=reads=begins=ends=0;failReadOpen=failWriteOpen=shortRead=shortWrite=false;}
template<class T> void set(const char* key,const T& value){
 const auto* first=reinterpret_cast<const unsigned char*>(&value);blobs[key]={first,first+sizeof(value)};
}
}
class Preferences {
 bool readonly_=true;
 public:
 bool begin(const char* name,bool readonly){
  ++preferences_test::begins;readonly_=readonly;
  return !strcmp(name,"tab5sidebar")&&!(readonly?preferences_test::failReadOpen:preferences_test::failWriteOpen);
 }
 size_t getBytesLength(const char* key){auto it=preferences_test::blobs.find(key);return it==preferences_test::blobs.end()?0:it->second.size();}
 size_t getBytes(const char* key,void* target,size_t size){
  ++preferences_test::reads;auto it=preferences_test::blobs.find(key);if(it==preferences_test::blobs.end())return 0;
  size_t count=std::min(size,it->second.size());if(preferences_test::shortRead&&count)--count;
  memcpy(target,it->second.data(),count);return count;
 }
 size_t putBytes(const char* key,const void* source,size_t size){
  ++preferences_test::writes;if(readonly_)return 0;
  if(preferences_test::shortWrite)return size?size-1:0;
  const auto* first=static_cast<const unsigned char*>(source);preferences_test::blobs[key]={first,first+size};return size;
 }
 void end(){++preferences_test::ends;}
};
