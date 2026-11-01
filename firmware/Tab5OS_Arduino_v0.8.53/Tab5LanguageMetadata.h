#pragma once
#include <stddef.h>
#include <string.h>

// Packs put their small catalogue fields before the translations object.
// Find the boundary without interpreting values, including quoted names and
// nested custom metadata. The JSON parser still validates the returned prefix.
// Returning zero asks the caller to use its normal complete-file parser.
namespace Tab5LanguageMetadata {
inline size_t prefixLength(const char* bytes,size_t size) {
  unsigned depth=0;
  bool quoted=false,escaped=false,key=false;
  size_t start=0,delimiter=0;
  char previous=0;
  for(size_t i=0;i<size;++i) {
    const char c=bytes[i];
    if(quoted) {
      if(escaped){escaped=false;continue;}
      if(c=='\\'){escaped=true;continue;}
      if(c!='"')continue;
      quoted=false;
      if(key && i-start==12 && !memcmp(bytes+start,"translations",12)) {
        size_t next=i+1;
        while(next<size && (bytes[next]==' '||bytes[next]=='\t'||bytes[next]=='\r'||bytes[next]=='\n'))++next;
        if(next<size && bytes[next]==':' && delimiter && bytes[delimiter]==',')return delimiter;
      }
      previous='"';
      continue;
    }
    if(c==' '||c=='\t'||c=='\r'||c=='\n')continue;
    if(c=='"'){quoted=true;escaped=false;start=i+1;key=depth==1&&(previous=='{'||previous==',');continue;}
    if(c=='{'||c=='[')++depth;
    else if(c=='}'||c==']'){if(!depth)return 0;--depth;}
    if(depth==1 && c==',')delimiter=i;
    previous=c;
  }
  return 0;
}
}
