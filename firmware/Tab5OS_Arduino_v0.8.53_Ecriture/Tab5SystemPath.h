#pragma once
#include <stddef.h>
#include <string.h>
// Legacy logical SD root only. USB and similarly named user folders stay intact.
// C-compatible for the MicroPython adapter; supports overlapping buffers.
static inline int tab5LegacySystemPath(const char* path,size_t length) {
 static const char old[]="/m5writer";
 if(!path||length<9||(length>9&&path[9]!='/'))return 0;
 for(size_t i=0;i<9;++i){unsigned char c=(unsigned char)path[i];if(c>='A'&&c<='Z')c+=32;if(c!=(unsigned char)old[i])return 0;}
 return 1;
}
static inline int tab5SystemPath(char* output,size_t capacity,const char* path,size_t length) {
 if(!output||!path)return 0;
 const int legacy=tab5LegacySystemPath(path,length);
 const size_t size=length-(legacy?2:0);
 if(size>=capacity)return 0;
 if(legacy){memmove(output+7,path+9,length-9);memcpy(output,"/System",7);}
 else memmove(output,path,length);
 output[size]=0;return 1;
}
// Resolve volume-absolute paths before identifying their first component.
// Preserve filename case. Name validation belongs to the backing filesystem,
// rather than imposing the stricter command FilePolicy on every FS operation.
// Scratch must not overlap input; output may overlap either input or scratch.
static inline int tab5CanonicalSystemPathBuffer(char* output,size_t capacity,const char* path,size_t length,char* normalized,size_t normalizedCapacity) {
 if(!output||!path||!normalized||!length||length>=normalizedCapacity||(path[0]!='/'&&path[0]!='\\'))return 0;
 size_t used=1;normalized[0]='/';
 for(size_t start=1;start<=length;){
  size_t end=start;
  while(end<length&&path[end]!='/'&&path[end]!='\\'){
   ++end;
  }
  size_t count=end-start;
  if(count==2&&path[start]=='.'&&path[start+1]=='.'){
   if(used==1)return 0;
   while(used>1&&normalized[used-1]!='/')--used;
   if(used>1)--used;
  }else if(count&&!(count==1&&path[start]=='.')){
   if(used>1)normalized[used++]='/';
   memcpy(normalized+used,path+start,count);used+=count;
  }
  if(end==length)break;
  start=end+1;
 }
 // The physical USB mount prefix is case-sensitive even though FAT names are
 // not; canonical callers may already have lowercased this first component.
 if(used>=4&&(used==4||normalized[4]=='/')&&
     (normalized[1]=='u'||normalized[1]=='U')&&
     (normalized[2]=='s'||normalized[2]=='S')&&
     (normalized[3]=='b'||normalized[3]=='B'))memcpy(normalized,"/USB",4);
 return tab5SystemPath(output,capacity,normalized,used);
}
// MicroPython retains its existing bounded path contract and overlap support.
static inline int tab5CanonicalSystemPath(char* output,size_t capacity,const char* path,size_t length) {
 if(length>260)return 0;
 char normalized[264];
 return tab5CanonicalSystemPathBuffer(output,capacity,path,length,normalized,sizeof(normalized));
}
// POSIX import paths are already rooted under /sd. Keep that physical prefix
// separate from the logical alias, including an independent USB namespace.
static inline int tab5SystemPosixPath(char* output,size_t capacity,const char* path) {
 if(!path||!output||capacity<4||strncmp(path,"/sd",3)||(path[3]&&path[3]!='/'))return 0;
 const char* logical=path[3]?path+3:"/";
 const size_t length=strlen(logical);
 char converted[264];
 if(!tab5CanonicalSystemPath(converted,sizeof(converted),logical,length))return 0;
 size_t size=strlen(converted);if(size+3>=capacity)return 0;
 memmove(output+3,converted,size+1);memcpy(output,"/sd",3);return 1;
}
static inline int tab5SystemPythonPath(char* output,size_t capacity,const char* path) {
 if(!output||!path||!*path||strlen(path)>260)return 0;
 if(!strncmp(path,"/sd",3)&&(!path[3]||path[3]=='/'))return tab5SystemPosixPath(output,capacity,path);
 char logical[280];size_t length=strlen(path);
 if(path[0]=='/')memcpy(logical,path,length+1);
 else {
  static const char prefix[]="/MicroPython/";
  memcpy(logical,prefix,sizeof(prefix)-1);memcpy(logical+sizeof(prefix)-1,path,length+1);
  length+=sizeof(prefix)-1;
 }
 char converted[264];
 if(!tab5CanonicalSystemPath(converted,sizeof(converted),logical,length))return 0;
 size_t size=strlen(converted);if(size+3>=capacity)return 0;
 memmove(output+3,converted,size+1);memcpy(output,"/sd",3);return 1;
}
