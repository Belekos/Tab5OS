#pragma once
#include <string>
#include <cstring>
#include <cstdint>
class String {
 std::string s;
 public:
 String()=default;String(const char* x):s(x?x:""){};String(std::string x):s(std::move(x)){}
 const char* c_str()const{return s.c_str();}operator const char*()const{return c_str();}
 size_t length()const{return s.size();}
 bool startsWith(const char* x)const{return s.rfind(x,0)==0;}
 bool operator==(const char* b)const{return s==(b?b:"");}bool operator!=(const char* b)const{return !(*this==b);}
 bool operator==(const String& b)const{return s==b.s;}bool operator!=(const String& b)const{return s!=b.s;}
 String operator+(const char* b)const{return s+(b?b:"");}
 String operator+(const String& b)const{return s+b.s;}
};
