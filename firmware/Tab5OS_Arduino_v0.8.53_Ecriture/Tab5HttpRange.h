#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace Tab5HttpRange {
struct Range { uint64_t first=0,last=0,total=0; };
inline bool number(const char*& p,uint64_t& n) {
 n=0;if(*p<'0'||*p>'9')return false;
 do { unsigned d=unsigned(*p++-'0');if(n>(UINT64_MAX-d)/10)return false;n=n*10+d; } while(*p>='0'&&*p<='9');
 return true;
}
inline bool parse(const char* text,Range& r) {
 if(!text||strncmp(text,"bytes ",6))return false;
 const char* p=text+6;
 return number(p,r.first)&&*p++=='-'&&number(p,r.last)&&*p++=='/'&&number(p,r.total)&&!*p&&r.first<=r.last&&r.last<r.total;
}
inline bool matches(const char* text,uint64_t offset,uint32_t count,uint64_t expected,Range& r) {
 return count&&offset<=UINT64_MAX-count&&parse(text,r)&&r.first==offset&&r.last==offset+count-1&&(!expected||r.total==expected);
}
inline bool safeHeader(const char* s,size_t max=256) {
 if(!s)return false;size_t n=0;for(;*s;++s)if(++n>max||uint8_t(*s)<32||uint8_t(*s)==127)return false;return true;
}
inline bool strongEtag(const char* s) {
 size_t n=s?strlen(s):0;
 if(n<2||n>256||s[0]!='"'||s[n-1]!='"')return false;
 for(size_t i=1;i+1<n;++i){uint8_t c=uint8_t(s[i]);if(c<0x21||c=='"'||c==0x7f)return false;}
 return true;
}
}
