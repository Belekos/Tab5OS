#pragma once
#include <stddef.h>
#include <string.h>
#include <initializer_list>
#include "Tab5FileSharePage.h"

namespace Tab5WebLocale {
// A translated pack is data, including when the user edits it. Escaping '<'
// prevents HTML from closing the script before JavaScript parses its JSON.
struct Writer {
 char* output;size_t capacity,used=0;bool ok=true;
 bool append(const char* text,size_t length){
  if(!ok||!output||used>=capacity||length>=capacity-used){ok=false;return false;}
  memcpy(output+used,text,length);used+=length;output[used]=0;return true;
 }
 bool append(const char* text){return append(text,strlen(text));}
 bool quoted(const char* text){
  if(!append("\""))return false;
  for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p&&ok;++p){
   if(*p=='"'||*p=='\\'){char escape[]={'\\',char(*p)};append(escape,2);}
   else if(*p<32||*p=='<'||*p=='>'||*p=='&'){
    constexpr char hex[]="0123456789abcdef";char escape[]={'\\','u','0','0',hex[*p>>4],hex[*p&15]};append(escape,6);
   }else if(*p==0xe2&&p[1]==0x80&&(p[2]==0xa8||p[2]==0xa9)){append(p[2]==0xa8?"\\u2028":"\\u2029");p+=2;}
   else append(reinterpret_cast<const char*>(p),1);
  }return ok&&append("\"");
 }
};
inline bool rtl(const char* language){
 if(!language)return false;
 for(const char* code:{"ar","he","fa","ur"})if(!strncmp(language,code,2)&&(!language[2]||language[2]=='-'))return true;
 return false;
}
template<class Translate>
size_t page(char* output,size_t capacity,const char* language,char* scratch,size_t scratchSize,Translate translate){
 if(!output||!capacity||!scratch||!scratchSize)return 0;output[0]=0;
 Writer writer{output,capacity};writer.append(FileSharePageBefore);writer.append("{\"language\":");writer.quoted(language?language:"en-US");writer.append(",\"direction\":");writer.quoted(rtl(language)?"rtl":"ltr");
 for(const auto& entry:FileShareTexts){
  if(translate(entry.source,scratch,scratchSize)>=scratchSize){output[0]=0;return 0;}
  writer.append(",");writer.quoted(entry.id);writer.append(":");writer.quoted(scratch);
 }
 writer.append("}");writer.append(FileSharePageAfter);
 if(!writer.ok){output[0]=0;return 0;}return writer.used;
}
}
