#pragma once
#include <string>
#include <cstring>
#include <stdint.h>
#include <stddef.h>
class String {
 public:
  String()=default;String(const char* s):v(s?s:""){}String(const std::string& s):v(s){}
  const char* c_str()const{return v.c_str();}size_t length()const{return v.size();}
  bool startsWith(const char* s)const{return v.compare(0,strlen(s),s)==0;}
  void concat(const char* s,size_t n){v.append(s,n);}String& operator+=(char c){v+=c;return *this;}
  friend bool operator==(const String& a,const String& b){return a.v==b.v;}
  friend bool operator!=(const String& a,const String& b){return !(a==b);}
  friend String operator+(const String& a,const String& b){return a.v+b.v;}
 private:std::string v;
};
