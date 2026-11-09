#include "platform.h"
#include "../common/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
const struct tab5_app_api7 *dos_api;
char dos_mount_root[256];
struct dos_directory {int handle,failed;char path[256];struct dirent entry;};
static char last_path[512];static struct stat last_stat;
DIR *opendir(const char *path){
 if(!path||strlen(path)>=256){puts("DOS: folder path exceeds the supported length.");return NULL;}
 int h=dos_api->directory_open(path);if(h<=0){puts("DOS: cannot read the selected folder.");return NULL;}
 DIR *d=calloc(1,sizeof(*d));if(!d){tab5_api->base.close(h);return NULL;}
 d->handle=h;strcpy(d->path,path);return d;
}
struct dirent *readdir(DIR *d){
 if(!d)return NULL;if(d->failed)return NULL;
 if(tab5_api->base.should_stop()){d->failed=1;return NULL;}
 struct tab5_app_directory_entry e={0};e.size=sizeof(e);
 int n=dos_api->directory_next(d->handle,&e);
 if(n<0){d->failed=1;puts("DOS: folder read failed or exceeded the directory limits.");return NULL;}
 if(!n)return NULL;
 memcpy(d->entry.d_name,e.name,sizeof(d->entry.d_name));d->entry.d_name[255]=0;
 snprintf(last_path,sizeof(last_path),"%s%s%s",d->path,!strcmp(d->path,"/")?"":"/",e.name);
 last_stat.st_mode=(e.flags&1)?S_IFDIR:S_IFREG;last_stat.st_size=e.file_size;last_stat.st_mtime=0;
 return &d->entry;
}
int dos_directory_error(DIR *d){return !d||d->failed;}
int closedir(DIR *d){if(!d)return -1;int r=tab5_api->base.close(d->handle);free(d);return r;}
int stat(const char *path,struct stat *out){
 if(!path||!out)return -1;if(!strcmp(path,last_path)){*out=last_stat;return 0;}
 memset(out,0,sizeof(*out));if(!strcmp(path,dos_mount_root)){out->st_mode=S_IFDIR;return 0;}
 int h=tab5_api->base.open_read(path);if(h<0)return -1;out->st_mode=S_IFREG;out->st_size=tab5_api->file_size(h);tab5_api->base.close(h);return 0;
}
int posix_memalign(void **out,size_t align,size_t size){
 if(!out||align<sizeof(void*)||(align&(align-1))||size>SIZE_MAX-align)return -1;
 /* Page allocations live until the application arena is reclaimed by the OS. */
 void *p=malloc(size+align-1);if(!p)return -1;*out=(void*)(((uintptr_t)p+align-1)&~(uintptr_t)(align-1));return 0;
}
struct tm *localtime_r(const time_t *t,struct tm *out){struct tm *r=localtime(t);if(r)*out=*r;else{memset(out,0,sizeof(*out));out->tm_year=126;out->tm_mday=1;}return out;}
int fprintf(FILE *f,const char *format,...){(void)f;char b[512];va_list ap;va_start(ap,format);int n=vsnprintf(b,sizeof(b),format,ap);va_end(ap);tab5_api->base.log(b,strlen(b));return n;}
int fflush(FILE *f){(void)f;return 0;}
