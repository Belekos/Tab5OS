#pragma once
#include <string>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
namespace tab5http {
inline std::string lower(std::string s){for(auto& c:s)if(c>='A'&&c<='Z')c+=32;return s;}
struct Reply {int status=0;long length=-1;bool chunked=false,upgrade=false,connectionUpgrade=false,close=false;};
// Read exactly through CRLF CRLF: never consume the first TLS record as HTTP.
template<class Client,class Clock,class Pause,class Cancel>
bool line(Client& c,std::string& out,Clock clock,Pause pause,Cancel cancel,uint32_t start,size_t& total){
 out.clear();while(uint32_t(clock()-start)<15000&&!cancel()){
  if(c.available()){int b=c.read();if(b<0)continue;if(++total>8192)return false;
   if(b=='\n'){if(out.empty()||out.back()!='\r')return false;out.pop_back();return true;}
   if(out.size()>=1024)return false;out+=char(b);
  }else if(!c.connected())return false;else pause();
 }return false;
}
template<class Client,class Clock,class Pause,class Cancel>
bool headers(Client& c,Reply& r,Clock clock,Pause pause,Cancel cancel){
 uint32_t start=clock();size_t total=0;std::string s;
 if(!line(c,s,clock,pause,cancel,start,total)||s.size()<12||s.compare(0,9,"HTTP/1.1 "))return false;
 if(s[9]<'1'||s[9]>'5'||s[10]<'0'||s[10]>'9'||s[11]<'0'||s[11]>'9')return false;
 r.status=(s[9]-'0')*100+(s[10]-'0')*10+s[11]-'0';
 while(line(c,s,clock,pause,cancel,start,total)){
  if(s.empty())return true;auto sep=s.find(':');if(sep==std::string::npos)return false;
  auto name=lower(s.substr(0,sep)),value=lower(s.substr(sep+1));
  if(name=="upgrade")r.upgrade=value.find("tls/")!=std::string::npos;
  if(name=="connection"){r.connectionUpgrade=value.find("upgrade")!=std::string::npos;r.close=value.find("close")!=std::string::npos;}
  if(name=="transfer-encoding")r.chunked=value.find("chunked")!=std::string::npos;
  if(name=="content-length"){char* end=nullptr;long n=strtol(value.c_str(),&end,10);while(end&&*end==' ')++end;if(!end||*end||n<0||n>65536)return false;if(r.length>=0&&r.length!=n)return false;r.length=n;}
 }return false;
}
template<class Client,class Clock,class Pause,class Cancel>
bool drain(Client& c,const Reply& r,Clock clock,Pause pause,Cancel cancel){
 if(r.close)return false;
 uint32_t start=clock();size_t total=0;std::string s;
 auto bytes=[&](size_t count){while(count&&uint32_t(clock()-start)<15000&&!cancel()){
  if(c.available()){if(c.read()>=0)--count;}else if(!c.connected())return false;else pause();
 }return count==0;};
 if(r.chunked){size_t body=0;
  for(;;){if(!line(c,s,clock,pause,cancel,start,total))return false;char* end=nullptr;unsigned long n=strtoul(s.c_str(),&end,16);if(end==s.c_str()||(*end&&*end!=';')||n>65536-body)return false;
   if(!n){do{if(!line(c,s,clock,pause,cancel,start,total))return false;}while(!s.empty());return true;}
   body+=n;if(!bytes(n)||!line(c,s,clock,pause,cancel,start,total)||!s.empty())return false;
  }
 }return r.length>=0&&bytes(size_t(r.length));
}
}
