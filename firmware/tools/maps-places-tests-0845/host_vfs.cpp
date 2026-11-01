#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string>
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <cerrno>
extern "C" FILE* __real_fopen(const char*,const char*);
extern "C" int __real_stat(const char*,struct stat*);
extern "C" DIR* __real_opendir(const char*);
extern "C" int __real_mkdir(const char*,mode_t);
extern "C" int __real_rename(const char*,const char*);
extern "C" int __real_unlink(const char*);
extern "C" int __real_rmdir(const char*);
std::string testRoot,failOpen;
int failOpenError=EMFILE,failMalloc=0,failRead=0,failStat=0,failClose=0,failPublish=0;
unsigned writeOpens=0,renames=0,removes=0;
static std::string mapped(const char* p){return std::string(p).rfind("/sd",0)==0?testRoot+(p+3):std::string(p);}
extern "C" FILE* __wrap_fopen(const char* p,const char* m){if(failOpen==p){errno=failOpenError;return nullptr;}if(m[0]=='w')++writeOpens;return __real_fopen(mapped(p).c_str(),m);}
extern "C" int __wrap_stat(const char* p,struct stat* s){if(failStat){errno=failStat;return -1;}return __real_stat(mapped(p).c_str(),s);}
extern "C" DIR* __wrap_opendir(const char* p){return __real_opendir(mapped(p).c_str());}
extern "C" int __wrap_mkdir(const char* p,mode_t m){return __real_mkdir(mapped(p).c_str(),m);}
extern "C" int __wrap_rename(const char* a,const char* b){++renames;if(failPublish){errno=EIO;return -1;}return __real_rename(mapped(a).c_str(),mapped(b).c_str());}
extern "C" int __wrap_unlink(const char* p){++removes;return __real_unlink(mapped(p).c_str());}
extern "C" int __wrap_rmdir(const char* p){return __real_rmdir(mapped(p).c_str());}
extern "C" void* __real_malloc(size_t);
extern "C" void* __wrap_malloc(size_t n){if(failMalloc&&!--failMalloc){errno=0;return nullptr;}return __real_malloc(n);}

extern "C" size_t __real_fread(void*,size_t,size_t,FILE*);
extern "C" size_t __wrap_fread(void* p,size_t n,size_t c,FILE*f){if(failRead){errno=EIO;return 0;}return __real_fread(p,n,c,f);}
extern "C" int __real_fclose(FILE*);
extern "C" int __wrap_fclose(FILE*f){int r=__real_fclose(f);if(failClose){errno=EIO;return EOF;}return r;}
