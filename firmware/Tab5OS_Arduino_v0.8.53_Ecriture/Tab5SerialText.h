#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
namespace Tab5Serial {
// Last 16 KiB of readable text. Incremental UTF-8 decoder survives packet
// boundaries; binary controls/invalid sequences cannot enter LVGL labels.
struct TextCapture {
 static constexpr size_t Capacity=16384;
 char data[Capacity+1]{};size_t size=0;uint32_t revision=0,trimmed=0,invalid=0;
 char partial[4]{};unsigned partialSize=0,expected=0;bool afterCR=false;
 void clear(){size=0;data[0]=0;partialSize=expected=0;trimmed=invalid=0;afterCR=false;++revision;}
 void append(const char* text,size_t n){
  if(size+n>Capacity){size_t remove=std::max(size+n-Capacity,Capacity/4);while(remove<size&&(uint8_t(data[remove])&0xc0)==0x80)++remove;memmove(data,data+remove,size-remove);size-=remove;trimmed+=uint32_t(remove);}
  memcpy(data+size,text,n);size+=n;data[size]=0;++revision;
 }
 void replacement(){append("\xef\xbf\xbd",3);++invalid;}
 void finish(){if(partialSize){replacement();partialSize=expected=0;}}
 void feed(const void* input,size_t length){auto* bytes=static_cast<const uint8_t*>(input);
  for(size_t i=0;i<length;++i){uint8_t c=bytes[i];
   if(partialSize){
    if((c&0xc0)!=0x80){replacement();partialSize=expected=0;--i;continue;}
    partial[partialSize++]=char(c);if(partialSize<expected)continue;
    const uint8_t a=uint8_t(partial[0]),b=uint8_t(partial[1]);
    bool valid=(expected!=3||!((a==0xe0&&b<0xa0)||(a==0xed&&b>=0xa0)))&&(expected!=4||!((a==0xf0&&b<0x90)||(a==0xf4&&b>=0x90)));
    if(valid)append(partial,partialSize);else replacement();partialSize=expected=0;continue;
   }
   if(c=='\n'&&afterCR){afterCR=false;continue;}afterCR=false;
   if(c=='\r'){append("\n",1);afterCR=true;continue;}
   if(c=='\n'||c=='\t'||(c>=32&&c<127)){char ch=char(c);append(&ch,1);continue;}
   expected=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
   if(expected){partial[0]=char(c);partialSize=1;}else replacement();
  }
 }
};
}
