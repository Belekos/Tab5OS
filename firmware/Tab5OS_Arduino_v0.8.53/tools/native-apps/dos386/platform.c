#include "../common/runtime.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
/* The interpreter's peripherals use a monotonic host clock; no fixed guest
 * MHz cap is used. The ABI7 clock has millisecond resolution. */
uint32_t get_uticks(void){return tab5_api->base.millis()*1000u;}
void *bigmalloc(size_t n){return calloc(1,n);}
void *pcmalloc(long n){return n>0?calloc(1,(size_t)n):NULL;}
int clock_gettime(clockid_t clock,struct timespec *out){
 (void)clock;if(!out)return -1;uint32_t now=tab5_api->base.millis();
 out->tv_sec=now/1000u;out->tv_nsec=(long)(now%1000u)*1000000L;return 0;
}
int usleep(unsigned usec){(void)usec;return 0;}
/* libm error reporting is application-local; no newlib process state import. */
int *__errno(void){static int value;return &value;}
struct tm *gmtime_r(const time_t *value,struct tm *out){
 struct tm *result=localtime(value);if(!out)return NULL;
 if(result)*out=*result;else{memset(out,0,sizeof(*out));out->tm_year=126;out->tm_mday=1;}
 return out;
}
int load_rom(void *memory,const char *path,uint32_t address,int backward){
 int h=tab5_api->base.open_read(path);if(h<0){printf("Missing x86 BIOS: %s\n",path);abort();}
 uint32_t size=tab5_api->file_size(h);
 /* Only reset BIOS at top of first MiB and VGA ROM at C0000 are accepted. */
 int valid=backward?(address==0x100000u&&size>=32768&&size<=131072):
                   (address==0xc0000u&&size>=16384&&size<=65536);
 if(!valid){tab5_api->base.close(h);puts("Invalid x86 BIOS size or destination.");abort();}
 uint8_t *dest=(uint8_t*)memory+address-(backward?size:0);
 int ok=tab5_load(h,dest,size);tab5_api->base.close(h);
 if(!ok){puts("Cannot read x86 BIOS.");abort();}return (int)size;
}
/* The optional upstream INI frontend is not linked. This conversion supports
 * remaining PCI diagnostic/configuration helpers without a hosted libc. */
long strtol(const char *s,char **end,int base){
 const char *start=s;while(*s==' '||*s=='\t'||*s=='\n'||*s=='\r')s++;
 int negative=0;if(*s=='-'||*s=='+'){negative=*s=='-';s++;}
 if((base==0||base==16)&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')){base=16;s+=2;}
 if(!base)base=*s=='0'?8:10;
 if(base<2||base>36){if(end)*end=(char*)start;return 0;}
 unsigned long value=0,limit=negative?(unsigned long)LONG_MAX+1u:(unsigned long)LONG_MAX;
 int any=0,overflow=0;
 for(;;){unsigned char c=(unsigned char)*s;unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='z'?c-'a'+10:c>='A'&&c<='Z'?c-'A'+10:99;
  if(d>=(unsigned)base)break;any=1;
  if(value>(limit-d)/(unsigned)base)overflow=1;else if(!overflow)value=value*(unsigned)base+d;s++;}
 if(end)*end=(char*)(any?s:start);
 if(overflow){errno=ERANGE;return negative?LONG_MIN:LONG_MAX;}
 if(negative)return value==(unsigned long)LONG_MAX+1u?LONG_MIN:-(long)value;return (long)value;
}
