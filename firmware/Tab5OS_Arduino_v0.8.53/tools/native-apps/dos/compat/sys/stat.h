#pragma once
#include <stdint.h>
#include <time.h>
struct stat {uint32_t st_mode,st_size;time_t st_mtime;};
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_IFMT 0170000
#define S_ISDIR(mode) (((mode)&S_IFMT)==S_IFDIR)
#define S_ISREG(mode) (((mode)&S_IFMT)==S_IFREG)
int stat(const char *path,struct stat *out);
