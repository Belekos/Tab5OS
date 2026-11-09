#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "Tab5AppApi.h"

// Directory enumeration is confined to the launch argument's directory (or
// /DOS when there is no argument). All reads remain in the storage broker.
namespace Tab5AppDirectory {
constexpr size_t MaxPath=260,MaxEntries=1024;
inline bool validPath(const char* path){
 if(!path||path[0]!='/')return false;
 size_t n=0;while(n<=MaxPath&&path[n])++n;
 if(!n||n>MaxPath||(n>1&&path[n-1]=='/'))return false;
 if(n==1)return true;
 size_t start=1;
 for(size_t i=1;i<=n;++i){unsigned char c=path[i];
  if(i==n||c=='/'){
   size_t count=i-start;
   if(!count||(count==1&&path[start]=='.')||(count==2&&path[start]=='.'&&path[start+1]=='.')||path[i-1]==' '||path[i-1]=='.')return false;
   start=i+1;
  }else if(c<32||c==127||strchr("\\:*?\"<>|~",c))return false;
 }
 return true;
}
inline bool root(const char* argument,bool isDirectory,char* output){
 if(!argument||!argument[0]){memcpy(output,"/DOS",5);return true;}
 if(!validPath(argument))return false;
 size_t n=strlen(argument);
 if(!isDirectory){const char* slash=strrchr(argument,'/');n=slash==argument?1:size_t(slash-argument);}
 memcpy(output,argument,n);output[n]=0;return true;
}
inline bool contains(const char* root,const char* path){
 if(!validPath(root)||!validPath(path))return false;
 size_t n=strlen(root);
 return n==1||(!strncmp(root,path,n)&&(path[n]==0||path[n]=='/'));
}
// A backend may return a full path or basename. Do not truncate names, expose
// dot entries or silently omit oversized files: the caller gets an error.
inline bool entry(const char* name,bool directory,uint64_t bytes,tab5_app_directory_entry& out){
 if(!name||bytes>UINT32_MAX)return false;
 const char* slash=strrchr(name,'/');if(slash)name=slash+1;
 size_t n=0;while(n<sizeof(out.name)&&name[n])++n;
 if(!n||n>=sizeof(out.name)||!strcmp(name,".")||!strcmp(name,".."))return false;
 for(size_t i=0;i<n;++i)if(static_cast<unsigned char>(name[i])<32||name[i]=='\\')return false;
 out.size=sizeof(out);out.flags=directory?TAB5_DIRECTORY_ENTRY_DIRECTORY:0;
 out.file_size=directory?0:static_cast<uint32_t>(bytes);memcpy(out.name,name,n+1);return true;
}
}
