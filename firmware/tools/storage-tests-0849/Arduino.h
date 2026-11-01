#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <algorithm>
class String : public std::string {
 public:
 using std::string::string;
 String() = default;
 String(const std::string& s):std::string(s){}
 String(unsigned v):std::string(std::to_string(v)){}
 String substring(unsigned from,unsigned to)const{return substr(from,to-from);}
};
inline uint32_t millis(){static uint32_t now=100;return ++now;}
