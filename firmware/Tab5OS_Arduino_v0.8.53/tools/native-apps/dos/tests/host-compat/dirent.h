#pragma once
#ifdef _WIN32
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
struct dirent {char d_name[260];};
typedef struct {intptr_t handle;struct _finddata_t found;struct dirent entry;int first;} DIR;
static inline DIR *opendir(const char *path){char full[512];snprintf(full,sizeof(full),"%s/*",path);DIR *d=calloc(1,sizeof(*d));if(!d)return NULL;d->handle=_findfirst(full,&d->found);if(d->handle==-1){free(d);return NULL;}d->first=1;return d;}
static inline struct dirent *readdir(DIR *d){if(!d||(!d->first&&_findnext(d->handle,&d->found)))return NULL;d->first=0;snprintf(d->entry.d_name,sizeof(d->entry.d_name),"%s",d->found.name);return &d->entry;}
static inline int closedir(DIR *d){if(!d)return -1;int n=_findclose(d->handle);free(d);return n;}
#else
#include_next <dirent.h>
#endif
