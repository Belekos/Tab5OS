#pragma once
struct dirent {char d_name[256];};
typedef struct dos_directory DIR;
DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);
