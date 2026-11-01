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
extern "C" int __real_fflush(FILE*);
extern "C" int __real_fsync(int);
extern "C" char* __real_strdup(const char*);
std::string testRoot,failOpen,failStat,failDir;
int failOpenError=EMFILE,failStatError=ENOENT,failDirError=ENOMEM,failMalloc=0;
int injectedRemoveError=0;
bool clearStatErrno=false,failStrdup=false,failFlush=false,failSync=false,clearSyncErrno=false;
static std::string mapped(const char* p){return std::string(p).rfind("/sd",0)==0?testRoot+(p+3):std::string(p);}
extern "C" FILE* __wrap_fopen(const char* p,const char* m){if(failOpen==p){errno=failOpenError;return nullptr;}return __real_fopen(mapped(p).c_str(),m);}
extern "C" int __wrap_stat(const char* p,struct stat* s){if(failStat==p){errno=failStatError;return -1;}int rc=__real_stat(mapped(p).c_str(),s);if(rc==0&&clearStatErrno)errno=0;return rc;}
extern "C" DIR* __wrap_opendir(const char* p){if(failDir==p){errno=failDirError;return nullptr;}return __real_opendir(mapped(p).c_str());}
extern "C" int __wrap_mkdir(const char* p,mode_t m){return __real_mkdir(mapped(p).c_str(),m);}
extern "C" int __wrap_rename(const char* a,const char* b){return __real_rename(mapped(a).c_str(),mapped(b).c_str());}
extern "C" int __wrap_unlink(const char* p){int rc=__real_unlink(mapped(p).c_str());if(injectedRemoveError){errno=injectedRemoveError;return -1;}return rc;}
extern "C" int __wrap_rmdir(const char* p){return __real_rmdir(mapped(p).c_str());}
extern "C" void* __real_malloc(size_t);
extern "C" void* __wrap_malloc(size_t n){if(failMalloc&&!--failMalloc){errno=0;return nullptr;}return __real_malloc(n);}
