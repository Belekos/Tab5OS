#pragma once
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstring>
struct String:std::string{using std::string::string;String(const std::string& s):std::string(s){}String()=default;void toLowerCase(){std::transform(begin(),end(),begin(),[](unsigned char c){return char(tolower(c));});}bool startsWith(const char* s)const{return compare(0,strlen(s),s)==0;}bool endsWith(const char* s)const{return length()>=strlen(s)&&compare(length()-strlen(s),strlen(s),s)==0;}};
inline unsigned millis(){static unsigned value=0;return value+=100;}
