#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <vector>
#include <limits>
namespace Tab5UsbIppProtocol {
struct Interface {uint8_t number=0,alternate=0,in=0,out=0;uint16_t packetIn=0;bool valid=false;};
inline Interface interface(const uint8_t* data,size_t length){
 Interface result,current;
 if(!data||length<9||data[0]<9||data[1]!=2)return result;
 const size_t total=size_t(data[2])|(size_t(data[3])<<8);
 if(total>length||total<9||total>4096)return result;
 bool candidate=false;
 for(size_t at=0;at<total;){
  const size_t n=data[at];if(n<2||n>total-at)return {};
  const uint8_t* p=data+at;
  if(p[1]==4){
   if(current.valid)return current;
   if(n<9)return {};
   current={};current.number=p[2];current.alternate=p[3];
   candidate=p[5]==7&&p[6]==1&&p[7]==4;
  }else if(p[1]==5&&candidate){
   if(n<7)return {};
   const uint16_t packet=(uint16_t(p[4])|(uint16_t(p[5])<<8))&0x7ff;
   if((p[3]&3)==2&&(p[2]&15)&&packet&&packet<=512){
    if(p[2]&0x80){if(current.in)return {};current.in=p[2];current.packetIn=packet;}
    else {if(current.out)return {};current.out=p[2];}
    current.valid=current.in&&current.out;
   }
  }
  at+=n;
 }
 return current.valid?current:result;
}
enum class Error:uint8_t {None,Truncated,Headers,Framing,TooLarge};
inline bool equal(const char* a,const char* b){while(*a&&*b){unsigned char c=*a++;if(c>='A'&&c<='Z')c+=32;if(c!=static_cast<unsigned char>(*b++))return false;}return !*a&&!*b;}
inline char* trim(char* s){while(*s==' '||*s=='\t')++s;size_t n=strlen(s);while(n&&(s[n-1]==' '||s[n-1]=='\t'))s[--n]=0;return s;}
template<class Reader,class Body> Error response(Reader&& next,Body& body,int& status){
 constexpr size_t Limit=65536,HeaderLimit=8192;
 size_t metadata=0;char line[513];body.clear();status=0;
 auto readLine=[&]()->Error{size_t n=0;bool cr=false;for(;;){int c=next();if(c<0)return Error::Truncated;if(++metadata>HeaderLimit)return Error::Headers;
   if(c=='\n'){if(!cr)return Error::Headers;line[n]=0;return Error::None;}
   if(cr)return Error::Headers;
   if(c=='\r'){cr=true;continue;}
   if((c<32&&c!='\t')||c>126||n==sizeof(line)-1)return Error::Headers;line[n++]=char(c);
  }};
 for(unsigned interim=0;interim<3;++interim){
  Error e=readLine();if(e!=Error::None)return e;
  if(strncmp(line,"HTTP/1.1 ",9)&&strncmp(line,"HTTP/1.0 ",9))return Error::Headers;
  if(strlen(line)<12||line[9]<'1'||line[9]>'5'||line[10]<'0'||line[10]>'9'||line[11]<'0'||line[11]>'9'||(line[12]&&line[12]!=' '))return Error::Headers;
  status=(line[9]-'0')*100+(line[10]-'0')*10+(line[11]-'0');
  bool hasLength=false,chunked=false;size_t length=0;
  for(;;){e=readLine();if(e!=Error::None)return e;if(!*line)break;char* colon=strchr(line,':');if(!colon||colon==line)return Error::Headers;*colon=0;char* value=trim(colon+1);
   if(equal(line,"content-length")){
    if(hasLength||chunked||!*value)return Error::Framing;hasLength=true;
    for(const char* p=value;*p;++p){if(*p<'0'||*p>'9')return Error::Framing;if(length>(Limit-unsigned(*p-'0'))/10)return Error::TooLarge;length=length*10+unsigned(*p-'0');}
   }else if(equal(line,"transfer-encoding")){
    if(chunked||hasLength||!equal(value,"chunked"))return Error::Framing;chunked=true;
   }
  }
  if(status==100){if(chunked||(hasLength&&length))return Error::Framing;continue;}
  if(!hasLength&&!chunked)return Error::Framing; // USB has no TCP close delimiter.
  body.reserve(chunked?Limit:length);
  auto bytes=[&](size_t n)->Error{if(n>Limit-body.size())return Error::TooLarge;for(size_t i=0;i<n;++i){int c=next();if(c<0)return Error::Truncated;body.push_back(uint8_t(c));}return Error::None;};
  if(!chunked)return bytes(length);
  for(;;){
   e=readLine();if(e!=Error::None)return e;size_t chunk=0,digits=0;
   const char* p=line;for(;*p&&*p!=';';++p){unsigned value;if(*p>='0'&&*p<='9')value=*p-'0';else if(*p>='a'&&*p<='f')value=*p-'a'+10;else if(*p>='A'&&*p<='F')value=*p-'A'+10;else return Error::Framing;if(++digits>8||chunk>(Limit-value)/16)return Error::TooLarge;chunk=chunk*16+value;}
   if(!digits)return Error::Framing;
   if(!chunk){for(;;){e=readLine();if(e!=Error::None)return e;if(!*line)return Error::None;char* colon=strchr(line,':');if(!colon||colon==line)return Error::Headers;*colon=0;if(equal(line,"content-length")||equal(line,"transfer-encoding"))return Error::Framing;}}
   e=bytes(chunk);if(e!=Error::None)return e;if(next()!='\r'||next()!='\n')return Error::Framing;
  }
 }
 return Error::Headers;
}
}
