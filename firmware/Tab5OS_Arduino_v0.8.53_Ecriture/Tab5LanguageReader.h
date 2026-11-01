#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Tab5TranslationFormat.h"

// Language packs are a flat string dictionary, not a general-purpose JSON DOM.
// Decode in the caller's PSRAM buffer once, then sort the lookup table. This
// avoids ArduinoJson's repeated linear member/string searches for every label.
// The whole input is still validated before a caller can replace its language.
namespace Tab5LanguageReader {
struct Entry { const char* source; const char* translated; };
struct Pack { const char* id=nullptr; const char* name=nullptr; size_t count=0; };
class Parser {
 public:
  Parser(char* bytes,size_t size,Entry* entries,size_t capacity)
      :at_(bytes),end_(bytes+size),entries_(entries),capacity_(capacity){}
  bool parse(Pack& result) {
    if(!take('{'))return false;
    bool idSeen=false,nameSeen=false,translationsSeen=false;
    if(!take('}'))for(;;){
      char* key=nullptr;if(!string(key)||!take(':'))return false;
      if(!std::strcmp(key,"id")){
        if(idSeen)return false;idSeen=true;
        char* value=nullptr;if(!string(value))return false;pack_.id=value;
      }else if(!std::strcmp(key,"name")){
        if(nameSeen)return false;nameSeen=true;
        if(peek()=='"'){char* value=nullptr;if(!string(value))return false;pack_.name=value;}
        else if(!skip(1))return false;
      }else if(!std::strcmp(key,"translations")){
        if(translationsSeen)return false;translationsSeen=true;
        if(!dictionary())return false;
      }else if(!skip(1))return false;
      if(take('}'))break;
      if(!take(','))return false;
    }
    whitespace();
    if(at_!=end_||!pack_.id||!pack_.id[0]||std::strlen(pack_.id)>63||!translationsSeen)return false;
    if(!pack_.name)pack_.name=pack_.id;
    if(!pack_.name[0]||std::strlen(pack_.name)>127)return false;
    if(pack_.count)std::sort(entries_,entries_+pack_.count,[](const Entry& a,const Entry& b){return std::strcmp(a.source,b.source)<0;});
    size_t retained=0;
    for(size_t i=0;i<pack_.count;++i){
      if(i&&std::strcmp(entries_[i-1].source,entries_[i].source)==0)return false;
      const auto item=entries_[i];
      if(!item.translated||!item.translated[0])continue;
      if(!Tab5TranslationFormat::compatible(item.source,item.translated))return false;
      // An explicit identity translation is meaningful outside French: dropping
      // it would incorrectly choose the English fallback for a known label.
      if(std::strcmp(pack_.id,"fr-FR")||std::strcmp(item.source,item.translated))entries_[retained++]=item;
    }
    pack_.count=retained;result=pack_;return true;
  }
 private:
  char *at_,*end_;
  Entry* entries_;size_t capacity_;Pack pack_;
  void whitespace(){while(at_!=end_&&(*at_==' '||*at_=='\r'||*at_=='\n'||*at_=='\t'))++at_;}
  char peek(){whitespace();return at_==end_?0:*at_;}
  bool take(char c){whitespace();if(at_==end_||*at_!=c)return false;++at_;return true;}
  static int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
  bool unit(uint32_t& value){
    if(size_t(end_-at_)<4)return false;value=0;
    for(unsigned i=0;i<4;++i){const int n=hex(*at_++);if(n<0)return false;value=(value<<4)|unsigned(n);}return true;
  }
  static void utf8(char*& out,uint32_t value){
    if(value<0x80)*out++=char(value);
    else if(value<0x800){*out++=char(0xc0|(value>>6));*out++=char(0x80|(value&63));}
    else if(value<0x10000){*out++=char(0xe0|(value>>12));*out++=char(0x80|((value>>6)&63));*out++=char(0x80|(value&63));}
    else{*out++=char(0xf0|(value>>18));*out++=char(0x80|((value>>12)&63));*out++=char(0x80|((value>>6)&63));*out++=char(0x80|(value&63));}
  }
  bool string(char*& value){
    if(!take('"'))return false;
    value=at_;char* out=at_;
    while(at_!=end_){
      const unsigned char c=static_cast<unsigned char>(*at_++);
      if(c=='"'){*out=0;return true;}
      if(c<0x20)return false;
      if(c=='\\'){
        if(at_==end_)return false;
        const char escaped=*at_++;
        switch(escaped){
          case '"':case '\\':case '/':*out++=escaped;break;
          case 'b':*out++='\b';break;case 'f':*out++='\f';break;
          case 'n':*out++='\n';break;case 'r':*out++='\r';break;case 't':*out++='\t';break;
          case 'u':{
            uint32_t code;if(!unit(code)||!code)return false;
            if(code>=0xd800&&code<=0xdbff){
              if(size_t(end_-at_)<2||at_[0]!='\\'||at_[1]!='u')return false;at_+=2;
              uint32_t low;if(!unit(low)||low<0xdc00||low>0xdfff)return false;
              code=0x10000+((code-0xd800)<<10)+(low-0xdc00);
            }else if(code>=0xdc00&&code<=0xdfff)return false;
            utf8(out,code);break;
          }
          default:return false;
        }
      }else if(c<0x80)*out++=char(c);
      else{
        const unsigned extra=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;
        if(!extra||size_t(end_-at_)<extra)return false;
        uint32_t code=c&((1u<<(6-extra))-1);char* raw=at_-1;
        for(unsigned i=0;i<extra;++i){const unsigned char part=static_cast<unsigned char>(*at_++);if((part&0xc0)!=0x80)return false;code=(code<<6)|(part&63);}
        if((extra==1&&code<0x80)||(extra==2&&code<0x800)||(extra==3&&code<0x10000)||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;
        for(unsigned i=0;i<=extra;++i)*out++=raw[i];
      }
    }
    return false;
  }
  bool literal(const char* word){
    const size_t n=std::strlen(word);if(size_t(end_-at_)<n||std::memcmp(at_,word,n))return false;at_+=n;return true;
  }
  bool number(){
    if(at_!=end_&&*at_=='-')++at_;
    if(at_==end_)return false;
    if(*at_=='0')++at_;
    else{if(*at_<'1'||*at_>'9')return false;do{++at_;}while(at_!=end_&&*at_>='0'&&*at_<='9');}
    if(at_!=end_&&*at_=='.'){++at_;char* start=at_;while(at_!=end_&&*at_>='0'&&*at_<='9')++at_;if(at_==start)return false;}
    if(at_!=end_&&(*at_=='e'||*at_=='E')){++at_;if(at_!=end_&&(*at_=='+'||*at_=='-'))++at_;char* start=at_;while(at_!=end_&&*at_>='0'&&*at_<='9')++at_;if(at_==start)return false;}
    return true;
  }
  bool skip(unsigned depth){
    const char c=peek();
    if(c=='"'){char* unused=nullptr;return string(unused);}
    if(c=='{'||c=='['){
      if(depth>=10)return false;
      const bool object=c=='{';++at_;const char close=object?'}':']';
      if(take(close))return true;
      for(;;){if(object){char* key=nullptr;if(!string(key)||!take(':'))return false;}
        if(!skip(depth+1))return false;if(take(close))return true;if(!take(','))return false;}
    }
    if(c=='t')return literal("true");if(c=='f')return literal("false");if(c=='n')return literal("null");
    return c&&number();
  }
  bool dictionary(){
    if(!take('{'))return false;
    if(take('}'))return true;
    for(;;){
      if(pack_.count>=capacity_)return false;
      char *key=nullptr,*value=nullptr;if(!string(key)||!take(':'))return false;
      if(peek()=='"'){if(!string(value))return false;}else if(!skip(2))return false;
      entries_[pack_.count++]={key,value};
      if(take('}'))return true;if(!take(','))return false;
    }
  }
};
inline bool parse(char* bytes,size_t size,Entry* entries,size_t capacity,Pack& result){
  if(!bytes||!entries||!capacity)return false;
  return Parser(bytes,size,entries,capacity).parse(result);
}
}
