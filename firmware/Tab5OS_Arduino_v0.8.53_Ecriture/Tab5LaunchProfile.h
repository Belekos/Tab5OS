#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum class Tab5LaunchMode : uint8_t { Ask, None, File };
struct Tab5LaunchProfile { Tab5LaunchMode mode=Tab5LaunchMode::Ask; char argument[261]{}; };

// Public notation: sd:/Documents/file.ext and usb:/Documents/file.ext.
// Legacy logical /Documents and /USB/Documents remain accepted. No shell
// expansion, escaping, traversal or whitespace trimming is performed.
inline bool tab5LaunchPath(const char* input,char* output,size_t capacity){
 if(!input||!output||!capacity)return false;output[0]=0;
 const size_t inputSize=strnlen(input,266);if(!inputSize||inputSize>=266)return false;
 const char* path=input;bool usb=false,explicitSd=false;
 if(inputSize>=4&&(input[0]=='s'||input[0]=='S')&&(input[1]=='d'||input[1]=='D')&&input[2]==':'&&input[3]=='/'){path=input+3;explicitSd=true;}
 else if(inputSize>=5&&(input[0]=='u'||input[0]=='U')&&(input[1]=='s'||input[1]=='S')&&(input[2]=='b'||input[2]=='B')&&input[3]==':'&&input[4]=='/'){path=input+4;usb=true;}
 if(*path!='/')return false;
 const size_t size=strlen(path),prefix=usb?4:0;
 if(size==1){const char* root=usb?"/USB":"/";if(strlen(root)>=capacity)return false;strcpy(output,root);return true;}
 if(size+prefix>260||size+prefix>=capacity||path[size-1]=='/')return false;
 bool reserved=size>=4&&(path[1]=='U'||path[1]=='u')&&(path[2]=='S'||path[2]=='s')&&(path[3]=='B'||path[3]=='b')&&(size==4||path[4]=='/');
 if(explicitSd&&reserved)return false;
 size_t start=1;
 for(size_t i=1;i<=size;++i){const unsigned char c=path[i];
  if(i==size||c=='/'){const size_t n=i-start;if(!n||(n==1&&path[start]=='.')||(n==2&&path[start]=='.'&&path[start+1]=='.')||path[i-1]==' '||path[i-1]=='.')return false;start=i+1;}
  else if(c<32||c==127||strchr("\\:*?\"<>|~",c))return false;
 }
 if(usb)memcpy(output,"/USB",4);memcpy(output+prefix,path,size+1);
 if(!usb&&reserved)memcpy(output,"/USB",4);
 return true;
}
inline bool tab5LaunchDisplayPath(const char* input,char* output,size_t capacity){
 char path[261];if(!tab5LaunchPath(input,path,sizeof(path))||!output||!capacity)return false;
 const bool usb=!strncmp(path,"/USB/",5)||!strcmp(path,"/USB");const char* prefix=usb?"usb:":"sd:";const char* suffix=usb?(path[4]?path+4:"/"):path;
 const size_t a=strlen(prefix),b=strlen(suffix);if(a+b>=capacity)return false;
 memcpy(output,prefix,a);memcpy(output+a,suffix,b+1);return true;
}
bool tab5LaunchProfileLoad(const char* executable,Tab5LaunchProfile& profile);
bool tab5LaunchProfileSave(const char* executable,const Tab5LaunchProfile& profile);
bool tab5LaunchProfileForget(const char* executable);
