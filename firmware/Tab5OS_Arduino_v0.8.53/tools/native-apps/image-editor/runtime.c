/* Freestanding libc subset; the editor's allocator never aborts on failure. */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#include "../common/nanoprintf.h"
void* memset(void* p,int c,size_t n){unsigned char* d=p;while(n--)*d++=(unsigned char)c;return p;}
void* memcpy(void* p,const void* s,size_t n){unsigned char* d=p;const unsigned char* b=s;while(n--)*d++=*b++;return p;}
void* memmove(void* p,const void* s,size_t n){if((uintptr_t)p<(uintptr_t)s)return memcpy(p,s,n);unsigned char* d=p;const unsigned char* b=s;while(n){--n;d[n]=b[n];}return p;}
int memcmp(const void* a,const void* b,size_t n){const unsigned char* x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;++x;++y;}return 0;}
size_t strlen(const char* s){size_t n=0;while(s[n])++n;return n;}
int strcmp(const char* a,const char* b){while(*a&&*a==*b){++a;++b;}return (unsigned char)*a-(unsigned char)*b;}
int strncmp(const char* a,const char* b,size_t n){while(n--){if(*a!=*b||!*a)return (unsigned char)*a-(unsigned char)*b;++a;++b;}return 0;}
char* strchr(const char* s,int c){do{if(*s==(char)c)return (char*)s;}while(*s++);return NULL;}
char* strrchr(const char* s,int c){char* r=NULL;do{if(*s==(char)c)r=(char*)s;}while(*s++);return r;}
int snprintf(char* out,size_t cap,const char* fmt,...){va_list args;va_start(args,fmt);int r=npf_vsnprintf(out,cap,fmt,args);va_end(args);return r;}
int abs(int x){return x<0?-x:x;}
