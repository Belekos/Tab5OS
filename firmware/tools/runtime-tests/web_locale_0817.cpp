#include "Tab5WebLocale.h"
#include <cassert>
#include <cstdio>
#include <string>
int main(int argc,char** argv){
 const char* hostile="</script><script>alert(1)</script>\"\\\n\t&\xe2\x80\xa8\xe2\x80\xa9";
 char page[32768],scratch[1024];unsigned calls=0;
 const auto size=Tab5WebLocale::page(page,sizeof(page),"ar-SA",scratch,sizeof(scratch),[&](const char*,char* out,size_t cap){++calls;assert(strlen(hostile)<cap);strcpy(out,hostile);return strlen(hostile);});
 assert(size&&calls==42&&strlen(page)==size);
 assert(strstr(page,"\"direction\":\"rtl\"")&&strstr(page,"\\u003c/script\\u003e")&&strstr(page,"\\u2028\\u2029"));
 assert(!strstr(page,"<script>alert(1)"));
 assert(Tab5WebLocale::rtl("ar")&&Tab5WebLocale::rtl("he-IL")&&!Tab5WebLocale::rtl("arbitrary")&&!Tab5WebLocale::rtl("pt-BR"));
 const char* closing=strstr(page,"</script>");assert(closing&&!strstr(closing+1,"</script>"));
 if(argc>1){FILE* f=fopen(argv[1],"wb");assert(f);assert(fwrite(page,1,size,f)==size);fclose(f);}
 char tiny[4]={};assert(!Tab5WebLocale::page(tiny,sizeof(tiny),"en-US",scratch,sizeof(scratch),[](const char*,char*,size_t cap){return cap;}));assert(!*tiny);
 assert(!Tab5WebLocale::page(page,sizeof(page),"en-US",scratch,1,[](const char*,char*,size_t cap){return cap;}));assert(!*page);
 puts("PASS: translated snapshot, script-context escaping, RTL language matching, truncation rejection");
}
