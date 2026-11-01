#pragma once
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
class String {
 std::string value;
 public:
 String()=default;
 String(const char* s):value(s?s:""){}
 String(const std::string& s):value(s){}
 bool reserve(size_t size){value.reserve(size);return true;}
 size_t length()const{return value.size();}
 const char* c_str()const{return value.c_str();}
 char operator[](size_t i)const{return value[i];}
 String substring(size_t begin,size_t end)const{return value.substr(begin,end-begin);}
 String substring(size_t begin)const{return value.substr(begin);}
 int lastIndexOf(char c)const{auto pos=value.rfind(c);return pos==std::string::npos?-1:int(pos);}
 operator const char*()const{return value.c_str();}
 bool operator==(const String& other)const{return value==other.value;}
 int indexOf(char c)const{auto pos=value.find(c);return pos==std::string::npos?-1:int(pos);}
 long toInt()const{return std::strtol(value.c_str(),nullptr,10);}
 void trim(){auto start=value.find_first_not_of(" \t\r\n"),end=value.find_last_not_of(" \t\r\n");value=start==std::string::npos?"":value.substr(start,end-start+1);}
 bool startsWith(const char* prefix)const{return value.rfind(prefix,0)==0;}
 String& operator+=(const char* s){value+=s;return *this;}
 String& operator+=(char c){value+=c;return *this;}
 bool operator==(const char* s)const{return value==s;}
 friend String operator+(const String& a,const char* b){return a.value+b;}
};
