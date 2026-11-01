#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <sys/types.h>
using boolean=bool;
#define log_e(...) ((void)0)
#define log_w(...) ((void)0)
class String:public std::string {
 public:using std::string::string;
 String()=default;String(const std::string& s):std::string(s){}
 String(const char* p):std::string(p?p:""){}
 void trim(){size_t a=0,b=size();while(a<b&&uint8_t(at(a))<=32)++a;while(b>a&&uint8_t(at(b-1))<=32)--b;*this=String(substr(a,b-a));}
 void replace(char a,char b){std::replace(begin(),end(),a,b);}
 String(unsigned v):std::string(std::to_string(v)){}
 String substring(unsigned a,unsigned b)const{return substr(a,b-a);}
 bool startsWith(const char* p)const{return rfind(p,0)==0;}
 bool endsWith(const char* p)const{auto n=strlen(p);return size()>=n&&compare(size()-n,n,p)==0;}
};
class Print {int error=0;public:virtual ~Print()=default;virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t*,size_t)=0;int getWriteError()const{return error;}void setWriteError(int e=1){error=e;}void clearWriteError(){error=0;}};
class Stream:public Print {protected:unsigned _timeout=0;public:virtual int available()=0;virtual int read()=0;virtual int peek()=0;virtual void flush()=0;size_t readBytes(char* out,size_t n){size_t done=0;for(;done<n;++done){int v=read();if(v<0)break;out[done]=char(v);}return done;}};
inline const char* pathToFileName(const char* p){if(!p)return "";auto* last=strrchr(p,'/');return last?last+1:p;}
inline uint32_t millis(){static uint32_t time=100;return ++time;}
