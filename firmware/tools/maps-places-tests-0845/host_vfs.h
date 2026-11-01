#pragma once
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
FILE* host_fopen(const char*,const char*);
int host_stat(const char*,struct stat*);
DIR* host_opendir(const char*);
int host_mkdir(const char*,mode_t);
int host_rename(const char*,const char*);
int host_unlink(const char*);
int host_rmdir(const char*);
#define fopen host_fopen
#define stat host_stat
#define opendir host_opendir
#define mkdir host_mkdir
#define rename host_rename
#define unlink host_unlink
#define rmdir host_rmdir
