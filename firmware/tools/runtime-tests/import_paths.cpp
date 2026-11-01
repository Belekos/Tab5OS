#include <cstdio>
#include <cstring>
#include <cstddef>
static bool script_import_directory(const char* filename,char* out,size_t capacity){
 if(!filename||filename[0]!='/'||capacity<8)return false;
 size_t length=strnlen(filename,261);
 if(length<2||length>260||filename[length-1]=='/')return false;
 size_t used=3,lastSlash=3;memcpy(out,"/sd",3);
 const char* p=filename;
 while(*p){
  while(*p=='/')++p;
  const char* first=p;
  while(*p&&*p!='/'){
   if((unsigned char)*p<32||(unsigned char)*p==127||*p=='\\'||*p==':')return false;
   ++p;
  }
  const size_t count=(size_t)(p-first);if(!count)break;
  if((count==1&&first[0]=='.')||(count==2&&first[0]=='.'&&first[1]=='.'))return false;
  if(used>=capacity-1||count>capacity-used-2)return false;
  lastSlash=used;out[used++]='/';memcpy(out+used,first,count);used+=count;
 }
 out[lastSlash]=0;return used>3;
}

int main(){char out[320];unsigned n=0;
if(!script_import_directory("/MicroPython/Projets/test/main.py",out,sizeof(out))||strcmp(out,"/sd/MicroPython/Projets/test")){std::printf("FAIL case NaN");return 1;}++n;
if(!script_import_directory("/USB/demo/main.py",out,sizeof(out))||strcmp(out,"/sd/USB/demo")){std::printf("FAIL case NaN");return 1;}++n;
if(!script_import_directory("/main.py",out,sizeof(out))||strcmp(out,"/sd")){std::printf("FAIL case NaN");return 1;}++n;
if(!script_import_directory("//MicroPython///Projets/test//main.py",out,sizeof(out))||strcmp(out,"/sd/MicroPython/Projets/test")){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("relative.py",out,sizeof(out))){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("/../main.py",out,sizeof(out))){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("/MicroPython/./main.py",out,sizeof(out))){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("/USB/../../main.py",out,sizeof(out))){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("/bad\\folder/main.py",out,sizeof(out))){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("/bad:folder/main.py",out,sizeof(out))){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("/MicroPython/",out,sizeof(out))){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("/",out,sizeof(out))){std::printf("FAIL case NaN");return 1;}++n;
if(!script_import_directory("/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/b.py",out,sizeof(out))||strcmp(out,"/sd/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/b.py",out,sizeof(out))){std::printf("FAIL case NaN");return 1;}++n;
if(script_import_directory("/abc/d.py",out,8)){std::puts("FAIL short capacity");return 1;}std::printf("PASS %u import path cases + short capacity\n",n);return 0;}
