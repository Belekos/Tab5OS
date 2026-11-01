#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <functional>
#include <algorithm>
#include <chrono>
#include <thread>
#include <ctime>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
using ssize_t=long long;
#ifndef S_ISREG
#define S_ISREG(mode) (((mode)&_S_IFMT)==_S_IFREG)
#define S_ISDIR(mode) (((mode)&_S_IFMT)==_S_IFDIR)
#endif
#define F_OK 0
inline int fsync(int fd){return _commit(fd);}
inline int usleep(unsigned n){std::this_thread::sleep_for(std::chrono::microseconds(n));return 0;}
inline int mkdir(const char* p,int){return _mkdir(p);}
inline tm* localtime_r(const time_t* t,tm* out){return localtime_s(out,t)?nullptr:out;}
struct dirent {char d_name[512];};
struct DIR {std::vector<std::string> names;size_t next=0;dirent entry{};};
inline DIR* opendir(const char* path){std::error_code ec;std::filesystem::directory_iterator it(path,ec);if(ec)return nullptr;auto* dir=new DIR;for(const auto& entry:it)dir->names.push_back(entry.path().filename().string());return dir;}
inline dirent* readdir(DIR* dir){if(dir->next==dir->names.size())return nullptr;snprintf(dir->entry.d_name,sizeof(dir->entry.d_name),"%s",dir->names[dir->next++].c_str());return &dir->entry;}
inline int closedir(DIR* dir){delete dir;return 0;}
inline int fnmatch(const char* pattern,const char* text,int){
 if(!*pattern)return *text?1:0;
 if(*pattern=='*'){do{if(!fnmatch(pattern+1,text,0))return 0;}while(*text++);return 1;}
 if(*pattern=='?'&&*text)return fnmatch(pattern+1,text+1,0);
 return *pattern==*text&&*text?fnmatch(pattern+1,text+1,0):1;
}
