#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
struct Preferences {static std::map<std::string,std::vector<unsigned char>> values;static bool failWrite;std::string ns;bool begin(const char* n,bool){ns=n;return true;}void end(){}bool isKey(const char* n){return values.count(ns+"/"+n);}size_t getBytesLength(const char* n){return values[ns+"/"+n].size();}size_t getBytes(const char* n,void* p,size_t s){auto& v=values[ns+"/"+n];if(v.size()>s)return 0;memcpy(p,v.data(),v.size());return v.size();}size_t putBytes(const char* n,const void* p,size_t s){if(failWrite)return 0;auto* b=static_cast<const unsigned char*>(p);values[ns+"/"+n]={b,b+s};return s;}void remove(const char* n){values.erase(ns+"/"+n);}};
