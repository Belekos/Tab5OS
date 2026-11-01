#include "runtime.h"
#include "game_runtime.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
static int game_errno;
int* __errno(void){return &game_errno;}
char* strcat(char* dst,const char* src){strcpy(dst+strlen(dst),src);return dst;}
char* strncat(char* dst,const char* src,size_t n){char* p=dst+strlen(dst);while(n--&&*src)*p++=*src++;*p=0;return dst;}
int strcasecmp(const char* a,const char* b){return strncasecmp(a,b,SIZE_MAX);}
static int space(int c){return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f';}
unsigned long strtoul(const char* s,char** end,int base){const char* first=s;while(space(*s))++s;int negative=*s=='-';if(*s=='-'||*s=='+')++s;if((!base||base==16)&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')){base=16;s+=2;}if(!base)base=*s=='0'?8:10;const char* digits=s;unsigned long n=0;while(*s){unsigned x=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='z'?*s-'a'+10:*s>='A'&&*s<='Z'?*s-'A'+10:36;if(x>=(unsigned)base)break;if(n>(ULONG_MAX-x)/base){n=ULONG_MAX;game_errno=ERANGE;}else n=n*base+x;++s;}if(end)*end=(char*)(digits==s?first:s);return negative?0UL-n:n;}
long strtol(const char* s,char** end,int base){return (long)strtoul(s,end,base);}
int atoi(const char* s){return (int)strtol(s,NULL,10);}long atol(const char* s){return strtol(s,NULL,10);}
double strtod(const char* s,char** end){const char* start=s;while(space(*s))++s;int negative=*s=='-';if(*s=='-'||*s=='+')++s;double v=0;unsigned digits=0;while(*s>='0'&&*s<='9'){v=v*10+*s++-'0';++digits;}if(*s=='.'){++s;double scale=0.1;while(*s>='0'&&*s<='9'){v+=(*s++-'0')*scale;scale*=0.1;++digits;}}if(digits&&(*s=='e'||*s=='E')){char* next;long n=strtol(s+1,&next,10);if(next!=s+1){if(n>308)n=308;if(n<-308)n=-308;while(n>0){v*=10;--n;}while(n<0){v/=10;++n;}s=next;}}if(end)*end=(char*)(digits?s:start);return negative?-v:v;}
double atof(const char* s){return strtod(s,NULL);}
int vsscanf(const char* input,const char* format,va_list arguments){const char* p=input;int count=0;va_list ap;va_copy(ap,arguments);while(*format){if(space(*format)){while(space(*format))++format;while(space(*p))++p;continue;}if(*format!='%'){if(*format++!=*p++)break;continue;}++format;if(*format=='%'){++format;if(*p++!='%')break;continue;}int suppress=*format=='*';if(suppress)++format;unsigned width=0;while(*format>='0'&&*format<='9')width=width*10+*format++-'0';int lng=0;if(*format=='l'){lng=1;++format;if(*format=='l'){lng=2;++format;}}char type=*format++;if(!type)break;if(type=='n'){if(!suppress)*va_arg(ap,int*)=(int)(p-input);continue;}if(type!='c')while(space(*p))++p;if(!*p)break;
 if(type=='s'||type=='c'){unsigned n=0,limit=width?width:(type=='c'?1:4096);char* out=suppress?NULL:va_arg(ap,char*);while(*p&&n<limit&&(type=='c'||!space(*p))){if(out)out[n]=*p;++p;++n;}if(!n)break;if(out&&type=='s')out[n]=0;if(!suppress)++count;continue;}
 char tmp[128];unsigned n=0;while(p[n]&&!space(p[n])&&n<sizeof(tmp)-1&&(!width||n<width)){tmp[n]=p[n];++n;}tmp[n]=0;char* end;int fp=type=='f'||type=='g'||type=='e'||type=='F'||type=='G'||type=='E';if(fp){double v=strtod(tmp,&end);if(end==tmp)break;if(!suppress){if(lng)*va_arg(ap,double*)=v;else *va_arg(ap,float*)=v;}}else{unsigned long v=strtoul(tmp,&end,type=='x'||type=='X'?16:type=='o'?8:type=='i'?0:10);if(end==tmp)break;if(!suppress){if(lng==2)*va_arg(ap,long long*)=(long)v;else if(lng)*va_arg(ap,long*)=(long)v;else *va_arg(ap,int*)=(int)v;}}p+=end-tmp;if(!suppress)++count;}
 va_end(ap);return count;}
int fprintf(FILE* stream,const char* format,...){char buf[1024];va_list ap;va_start(ap,format);int n=vsnprintf(buf,sizeof(buf),format,ap);va_end(ap);size_t len=strlen(buf);if((uintptr_t)stream<=3)tab5_api->base.log(buf,len);else if(fwrite(buf,1,len,stream)!=len)return -1;return n;}
int fputs(const char* s,FILE* f){size_t n=strlen(s);if((uintptr_t)f<=3){tab5_api->base.log(s,n);return 0;}return fwrite(s,1,n,f)==n?0:EOF;}
int fputc(int c,FILE* f){unsigned char v=c;if((uintptr_t)f<=3){tab5_api->base.log((const char*)&v,1);return v;}return fwrite(&v,1,1,f)==1?v:EOF;}
int fgetc(FILE* f){unsigned char c;return fread(&c,1,1,f)==1?c:EOF;}
char* fgets(char* s,int n,FILE* f){if(n<1)return NULL;int i=0,c;while(i<n-1&&(c=fgetc(f))!=EOF){s[i++]=c;if(c=='\n')break;}s[i]=0;return i?s:NULL;}
int fflush(FILE* f){return (uintptr_t)f<=3?0:ferror(f)?EOF:0;}
int stat(const char* path,struct stat* out){if(!path||!out)return -1;memset(out,0,sizeof(*out));if(!strcmp(path,tab5_game_root())||!strcmp(path,tab5_game_saves())){out->st_mode=S_IFDIR|0700;return 0;}FILE* f=fopen(path,"rb");if(!f){game_errno=ENOENT;return -1;}fseek(f,0,SEEK_END);out->st_size=ftell(f);out->st_mode=S_IFREG|0600;fclose(f);return 0;}
int mkdir(const char* path,mode_t mode){(void)mode;if(!strcmp(path,tab5_game_saves()))return 0;game_errno=EROFS;return -1;}
int access(const char* path,int mode){(void)mode;struct stat st;return stat(path,&st);}
int remove(const char* path){(void)path;game_errno=EROFS;return -1;}
int rename(const char* a,const char* b){(void)a;(void)b;game_errno=EROFS;return -1;}
char* getenv(const char* name){(void)name;return NULL;}
char* getcwd(char* buffer,size_t size){size_t n=strlen(tab5_game_root())+1;if(n>size)return NULL;memcpy(buffer,tab5_game_root(),n);return buffer;}
void* memchr(const void* bytes,int c,size_t n){const unsigned char* p=bytes;while(n--){if(*p==(unsigned char)c)return (void*)p;++p;}return NULL;}
char* strtok(char* s,const char* delim){static char* next;if(s)next=s;if(!next)return NULL;while(*next&&strchr(delim,*next))++next;if(!*next)return NULL;char* result=next;while(*next&&!strchr(delim,*next))++next;if(*next)*next++=0;return result;}
long labs(long v){return v<0?-v:v;}
void setbuf(FILE* f,char* p){(void)f;(void)p;}
int fscanf(FILE* f,const char* format,...){
 /* The game configuration reader uses exactly two bounded string columns. */
 if(strcmp(format,"%79s %99[^\n]\n"))return EOF;
 char line[256];if(!fgets(line,sizeof(line),f))return EOF;char* p=line;while(space(*p))++p;
 va_list args;va_start(args,format);char* key=va_arg(args,char*);char* value=va_arg(args,char*);va_end(args);
 unsigned n=0;while(*p&&!space(*p)&&n<79)key[n++]=*p++;key[n]=0;if(!n)return 0;while(space(*p))++p;n=0;while(*p&&*p!='\n'&&*p!='\r'&&n<99)value[n++]=*p++;value[n]=0;return n?2:1;
}

int sscanf(const char* in,const char* format,...){va_list ap;va_start(ap,format);int n=vsscanf(in,format,ap);va_end(ap);return n;}
int vsprintf(char* out,const char* format,va_list ap){return vsnprintf(out,4096,format,ap);}
void* heap_caps_malloc(size_t size,uint32_t caps){(void)caps;return malloc(size);}
static FILE* game_fds[16];
int open(const char* path,int flags,...){if(flags&O_RDWR){game_errno=ENOTSUP;return -1;}for(unsigned i=3;i<16;++i)if(!game_fds[i]){game_fds[i]=fopen(path,(flags&O_WRONLY)?"wb":"rb");if(game_fds[i])return i;return -1;}game_errno=EMFILE;return -1;}
int close(int fd){if(fd<3||fd>=16||!game_fds[fd])return -1;FILE* f=game_fds[fd];game_fds[fd]=NULL;return fclose(f);}
ssize_t write(int fd,const void* data,size_t n){if(fd==1||fd==2){tab5_api->base.log(data,n);return n;}return fd>=3&&fd<16&&game_fds[fd]?(ssize_t)fwrite(data,1,n,game_fds[fd]):-1;}
ssize_t read(int fd,void* data,size_t n){return fd>=3&&fd<16&&game_fds[fd]?(ssize_t)fread(data,1,n,game_fds[fd]):-1;}
off_t lseek(int fd,off_t offset,int origin){return fd>=3&&fd<16&&game_fds[fd]&&!fseek(game_fds[fd],offset,origin)?ftell(game_fds[fd]):-1;}
int unlink(const char* path){return remove(path);}

int fstat(int fd,struct stat* st){if(fd<3||fd>=16||!game_fds[fd]||!st)return -1;FILE* f=game_fds[fd];long p=ftell(f);if(fseek(f,0,SEEK_END))return -1;memset(st,0,sizeof(*st));st->st_size=ftell(f);st->st_mode=S_IFREG|0600;return fseek(f,p,SEEK_SET);}
int vprintf(const char* format,va_list ap){char text[1024];int n=vsnprintf(text,sizeof(text),format,ap);tab5_api->base.log(text,strlen(text));return n;}
int putenv(char* value){(void)value;return 0;}
char* strerror(int code){(void)code;return "File operation failed";}
#include <time.h>
clock_t clock(void){return (clock_t)((uint64_t)tab5_api->base.millis()*CLOCKS_PER_SEC/1000);}
void _exit(int code){exit(code);}
