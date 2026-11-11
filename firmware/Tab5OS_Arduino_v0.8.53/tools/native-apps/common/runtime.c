#include "runtime.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#ifdef TAB5_GAME_RUNTIME
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
#else
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#endif
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#include "nanoprintf.h"
const struct tab5_app_api2* tab5_api;
intptr_t tab5_escape[5];
struct __attribute__((aligned(16))) block {size_t size;struct block* next;int used;};
static char gameRoot[256],gameSaves[80];static uint32_t gameId;
static struct block* heap;static size_t heapCapacity;static void (*reclaim_cache)(void);static int reclaiming;
void tab5_runtime_reclaimer(void (*callback)(void)){reclaim_cache=callback;}
void tab5_runtime_capacity(size_t bytes){if(!heap&&bytes>=65536&&bytes<=(tab5_api&&tab5_api->base.version>=4?16u:4u)*1024u*1024u)heapCapacity=(bytes+15)&~15u;}
void tab5_runtime_init(const struct tab5_app_api2* api){tab5_api=api;heap=NULL;heapCapacity=1024u*1024u;reclaim_cache=NULL;reclaiming=0;gameRoot[0]=gameSaves[0]=0;}
#if defined(TAB5_DOS)
/* DOS_HEAP_STATS_BEGIN
 * Read the application arena on its owning thread. This is not the OS's
 * physical PSRAM free count, and querying it never allocates the arena. */
void tab5_runtime_memory(size_t* capacity,size_t* available,size_t* largest){
 size_t total=0,biggest=0;
 if(!heap)total=biggest=heapCapacity-sizeof(struct block);
 else for(const struct block* b=heap;b;b=b->next)if(!b->used){total+=b->size;if(b->size>biggest)biggest=b->size;}
 if(capacity)*capacity=heapCapacity;if(available)*available=total;if(largest)*largest=biggest;
}
/* DOS_HEAP_STATS_END */
#endif
void abort(void){tab5_api->base.log("Application stopped.\n",21);__builtin_longjmp(tab5_escape,1);}
void exit(int code){(void)code;abort();}
/* DOS_MEMORY_PRIMITIVES_BEGIN
 * RV32 has inexpensive aligned 32-bit accesses. The DOS framebuffer and guest
 * RAM copies must not pay a byte load/store and loop branch for every byte.
 * may_alias keeps these accesses legal for pixels, structs and byte arrays;
 * unequal alignments stay on the byte path, and no read crosses the range. */
#if defined(TAB5_DOS)
#if defined(__GNUC__) || defined(__clang__)
typedef uint32_t dos_alias_word __attribute__((__may_alias__));
#else
typedef uint32_t dos_alias_word;
#endif
void* memset(void* p,int c,size_t n){
 unsigned char* d=p;uint32_t v=(uint32_t)(unsigned char)c*0x01010101u;
 while(n&&((uintptr_t)d&3u)){*d++=(unsigned char)c;--n;}
 while(n>=16){dos_alias_word* w=(void*)d;w[0]=v;w[1]=v;w[2]=v;w[3]=v;d+=16;n-=16;}
 while(n>=4){*(dos_alias_word*)(void*)d=v;d+=4;n-=4;}
 while(n--)*d++=(unsigned char)c;return p;
}
void* memcpy(void* p,const void* s,size_t n){
 unsigned char* d=p;const unsigned char* b=s;
 if(!(((uintptr_t)d^(uintptr_t)b)&3u)){
  while(n&&((uintptr_t)d&3u)){*d++=*b++;--n;}
  while(n>=16){dos_alias_word* w=(void*)d;const dos_alias_word* r=(const void*)b;w[0]=r[0];w[1]=r[1];w[2]=r[2];w[3]=r[3];d+=16;b+=16;n-=16;}
  while(n>=4){*(dos_alias_word*)(void*)d=*(const dos_alias_word*)(const void*)b;d+=4;b+=4;n-=4;}
 }
 while(n--)*d++=*b++;return p;
}
void* memmove(void* p,const void* s,size_t n){
 if(!n||p==s)return p;
 if((uintptr_t)p<(uintptr_t)s||(uintptr_t)p-(uintptr_t)s>=n)return memcpy(p,s,n);
 unsigned char* d=(unsigned char*)p+n;const unsigned char* b=(const unsigned char*)s+n;
 if(!(((uintptr_t)d^(uintptr_t)b)&3u)){
  while(n&&((uintptr_t)d&3u)){*--d=*--b;--n;}
  while(n>=16){d-=16;b-=16;dos_alias_word* w=(void*)d;const dos_alias_word* r=(const void*)b;w[3]=r[3];w[2]=r[2];w[1]=r[1];w[0]=r[0];n-=16;}
  while(n>=4){d-=4;b-=4;*(dos_alias_word*)(void*)d=*(const dos_alias_word*)(const void*)b;n-=4;}
 }
 while(n--)*--d=*--b;return p;
}
#else
void* memset(void* p,int c,size_t n){unsigned char* d=p;while(n--)*d++=(unsigned char)c;return p;}
void* memcpy(void* p,const void* s,size_t n){unsigned char* d=p;const unsigned char* b=s;while(n--)*d++=*b++;return p;}
void* memmove(void* p,const void* s,size_t n){if((uintptr_t)p<(uintptr_t)s)return memcpy(p,s,n);unsigned char* d=p;const unsigned char* b=s;while(n){--n;d[n]=b[n];}return p;}
#endif
/* DOS_MEMORY_PRIMITIVES_END */
int memcmp(const void* a,const void* b,size_t n){const unsigned char* x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;++x;++y;}return 0;}
size_t strlen(const char* s){size_t n=0;while(s[n])++n;return n;}
int strcmp(const char* a,const char* b){while(*a&&*a==*b){++a;++b;}return (unsigned char)*a-(unsigned char)*b;}
int strncmp(const char* a,const char* b,size_t n){while(n--){if(*a!=*b||!*a)return (unsigned char)*a-(unsigned char)*b;++a;++b;}return 0;}
char* strcpy(char* a,const char* b){memcpy(a,b,strlen(b)+1);return a;}
char* strncpy(char* a,const char* b,size_t n){size_t i=0;for(;i<n&&b[i];++i)a[i]=b[i];for(;i<n;++i)a[i]=0;return a;}
char* strchr(const char* s,int c){do{if(*s==(char)c)return (char*)s;}while(*s++);return NULL;}
char* strrchr(const char* s,int c){char* r=NULL;do{if(*s==(char)c)r=(char*)s;}while(*s++);return r;}
char* strstr(const char* s,const char* p){size_t n=strlen(p);for(;*s;++s)if(!strncmp(s,p,n))return (char*)s;return n?NULL:(char*)s;}
char* strdup(const char* s){size_t n=strlen(s)+1;char* p=malloc(n);if(p)memcpy(p,s,n);return p;}
static void memory_error(size_t n){size_t total=0,largest=0;for(struct block* b=heap;b;b=b->next)if(!b->used){total+=b->size;if(b->size>largest)largest=b->size;}printf("Memory limit: requested %u, free %u, largest %u, budget %u bytes.\n",(unsigned)n,(unsigned)total,(unsigned)largest,(unsigned)heapCapacity);abort();}
void* malloc(size_t n){
 if(!n)n=1;if(n>heapCapacity-2*sizeof(struct block))memory_error(n);n=(n+15u)&~15u;
 if(!heap){heap=tab5_api->base.allocate(heapCapacity);if(!heap)memory_error(n);*heap=(struct block){heapCapacity-sizeof(*heap),NULL,0};}
 int retried=0;
 retry:
 for(struct block* b=heap;b;b=b->next){if(b->used||b->size<n)continue;
  if(b->size>=n+sizeof(*b)+32){struct block* next=(void*)((char*)(b+1)+n);*next=(struct block){b->size-n-sizeof(*b),b->next,0};b->size=n;b->next=next;}
  b->used=1;return b+1;
 }if(reclaim_cache&&!reclaiming&&!retried){retried=1;reclaiming=1;reclaim_cache();reclaiming=0;goto retry;}memory_error(n);return NULL;
}
void free(void* p){if(!p)return;struct block* b=(struct block*)p-1;b->used=0;for(b=heap;b&&b->next;){if(!b->used&&!b->next->used){b->size+=sizeof(*b)+b->next->size;b->next=b->next->next;}else b=b->next;}}
void* calloc(size_t n,size_t m){if(m&&n>SIZE_MAX/m)return NULL;void* p=malloc(n*m);if(p)memset(p,0,n*m);return p;}
void* realloc(void* p,size_t n){if(!p)return malloc(n);if(!n){free(p);return NULL;}struct block* b=(struct block*)p-1;if(b->size>=n)return p;void* d=malloc(n);if(d){memcpy(d,p,b->size);free(p);}return d;}
int vsnprintf(char* out,size_t cap,const char* fmt,va_list args){return npf_vsnprintf(out,cap,fmt,args);}
int snprintf(char* out,size_t cap,const char* fmt,...){va_list args;va_start(args,fmt);int r=vsnprintf(out,cap,fmt,args);va_end(args);return r;}
int sprintf(char* out,const char* fmt,...){va_list args;va_start(args,fmt);int r=vsnprintf(out,4096,fmt,args);va_end(args);return r;}
int printf(const char* fmt,...){char out[512];va_list args;va_start(args,fmt);int r=vsnprintf(out,sizeof(out),fmt,args);va_end(args);tab5_api->base.log(out,strlen(out));return r;}
int puts(const char* s){tab5_api->base.log(s,strlen(s));tab5_api->base.log("\n",1);return 0;}
struct file {int fd,write,error,state;size_t pos,size,capacity;char path[256];uint8_t* data;};
#define SAVE_CAP ((tab5_api->base.version>=4?2u*1024u*1024u:512u*1024u)-16u)
struct state_header {uint32_t magic,length,path,crc;};
void tab5_runtime_game(const char* argument,const char* system){
 if(strlen(argument)>=sizeof(gameRoot)||strlen(system)>32)abort();strcpy(gameRoot,argument);char* slash=strrchr(gameRoot,'/');if(slash)*slash=0;else strcpy(gameRoot,"/");
 snprintf(gameSaves,sizeof(gameSaves),"/Saves/%s",system);gameId=tab5_crc32(0,argument,strlen(argument));
}
const char* tab5_game_root(void){return gameRoot;}
const char* tab5_game_saves(void){return gameSaves;}
static const char* game_path(const char* path,const char* mode,char* out,size_t cap){
 if(!gameSaves[0])return path;
 const char* ext=strrchr(path,'.');int save=mode[0]=='w'||(ext&&(!strcmp(ext,".cfg")||!strcmp(ext,".dsg")||!strcmp(ext,".sav")));
 if(save){const char* base=strrchr(path,'/');snprintf(out,cap,"%s/%08x-%s",gameSaves,(unsigned)gameId,base?base+1:path);return out;}
 if(path[0]!='/'){snprintf(out,cap,"%s/%s",gameRoot,path);return out;}return path;
}
FILE* fopen(const char* path,const char* mode){char resolved[256];path=game_path(path,mode,resolved,sizeof(resolved));if(strlen(path)>=256)return NULL;struct file* f=calloc(1,sizeof(*f));if(!f)return NULL;strcpy(f->path,path);f->write=mode[0]=='w';f->state=strlen(path)>6&&!strcmp(path+strlen(path)-6,".state");
 if(f->write){f->capacity=4096;f->data=calloc(1,f->capacity+16);if(!f->data){free(f);return NULL;}}
 else {f->fd=tab5_api->base.open_read(path);if(f->fd<0){free(f);return NULL;}f->size=tab5_api->file_size(f->fd);
 if(f->state){struct state_header h;uint8_t buf[1024];int ok=f->size>=16&&tab5_api->base.read(f->fd,&h,16)==16&&h.magic==0x33535435&&h.length==f->size-16&&h.length<=SAVE_CAP&&h.path==tab5_crc32(0,path,strlen(path));uint32_t c=0;size_t left=ok?h.length:0;while(left&&ok){size_t n=left>sizeof(buf)?sizeof(buf):left;int r=tab5_api->base.read(f->fd,buf,n);if(r!=(int)n){ok=0;break;}c=tab5_crc32(c,buf,n);left-=n;}if(!ok||c!=h.crc){tab5_api->base.close(f->fd);free(f);return NULL;}f->size=h.length;tab5_api->base.seek(f->fd,16);}
 }return (FILE*)f;}
size_t fread(void* ptr,size_t size,size_t count,FILE* stream){struct file* f=(void*)stream;if(!f||f->write||!size||count>SIZE_MAX/size)return 0;size_t n=size*count,done=0;if(n>f->size-f->pos)n=f->size-f->pos;
 while(done<n){size_t k=n-done;if(k>4096)k=4096;int r=tab5_api->base.read(f->fd,(char*)ptr+done,k);if(r<=0){f->error=1;break;}done+=r;if(tab5_api->base.should_stop())break;}
 f->pos+=done;return done/size;}
size_t fwrite(const void* ptr,size_t size,size_t count,FILE* stream){struct file* f=(void*)stream;if(!f||!f->write||!size||count>SIZE_MAX/size)return 0;size_t n=size*count;if(n>SAVE_CAP-f->pos){f->error=1;return 0;}if(f->pos+n>f->capacity){size_t cap=f->capacity;while(cap<f->pos+n){cap*=2;if(cap>SAVE_CAP){cap=SAVE_CAP;break;}}void* p=realloc(f->data,cap+16);if(!p){f->error=1;return 0;}f->data=p;memset(f->data+f->capacity+16,0,cap-f->capacity);f->capacity=cap;}memcpy(f->data+(f->state?16:0)+f->pos,ptr,n);f->pos+=n;if(f->size<f->pos)f->size=f->pos;return count;}
int fseek(FILE* stream,long offset,int whence){struct file* f=(void*)stream;if(!f)return -1;int64_t p=(int64_t)offset+(whence==SEEK_CUR?f->pos:whence==SEEK_END?f->size:0);if(p<0||p>(int64_t)(f->write?SAVE_CAP:f->size))return -1;if(!f->write&&tab5_api->base.seek(f->fd,p+(f->state?16:0)))return -1;f->pos=p;return 0;}
long ftell(FILE* stream){struct file* f=(void*)stream;return f?(long)f->pos:-1;}
int fclose(FILE* stream){struct file* f=(void*)stream;if(!f)return -1;int r=f->error?-1:0;if(f->write&&!r){if(f->state){struct state_header h={0x33535435,f->size,tab5_crc32(0,f->path,strlen(f->path)),tab5_crc32(0,f->data+16,f->size)};memcpy(f->data,&h,16);}r=tab5_api->save(f->path,f->data,f->size+(f->state?16:0));}if(!f->write)tab5_api->base.close(f->fd);free(f->data);free(f);return r;}
int feof(FILE* stream){struct file* f=(void*)stream;return !f||f->pos>=f->size;}
int ferror(FILE* stream){struct file* f=(void*)stream;return !f||f->error;}
int tab5_load(int h,void* data,size_t size){size_t off=0;while(off<size){size_t n=size-off;if(n>4096)n=4096;int r=tab5_api->base.read(h,(char*)data+off,n);if(r!=(int)n)return 0;off+=n;tab5_api->base.sleep_ms(0);}return 1;}
uint32_t tab5_crc32(uint32_t c,const void* data,size_t n){const uint8_t* p=data;c=~c;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
void tab5_save_name(char* out,const char* sys,const char* ext,uint32_t crc){snprintf(out,80,"/Saves/%s/%08x.%s",sys,(unsigned)crc,ext);}
void tab5_message(const char* en,const char* fr){puts(tab5_api->base.language[0]=='f'?fr:en);}
#ifndef TAB5_HOST_TEST
#undef memset
void* memset(void* p,int c,size_t n){return tab5_rt_memset(p,c,n);}
#undef memcpy
void* memcpy(void* p,const void* s,size_t n){return tab5_rt_memcpy(p,s,n);}
#undef memcmp
int memcmp(const void* a,const void* b,size_t n){return tab5_rt_memcmp(a,b,n);}
#undef memmove
void* memmove(void* p,const void* s,size_t n){return tab5_rt_memmove(p,s,n);}
#endif
#ifndef TAB5_HOST_TEST
/* DOS_DIVISION_PRIMITIVES_BEGIN
 * PIT/speaker arithmetic is uint64_t, but almost every per-sample numerator
 * fits in 32 bits after the timer rebases. Use the RV32 M division there.
 * Large operands need only bit_length(n)-bit_length(d)+1 subtract/shift steps,
 * rather than 64 steps. Remainders reuse that operation without a 64-bit mul. */
#if defined(TAB5_DOS)
int __clzsi2(unsigned x){
 if(!x)return 32;int n=0;
 if(!(x&0xffff0000u)){n+=16;x<<=16;}
 if(!(x&0xff000000u)){n+=8;x<<=8;}
 if(!(x&0xf0000000u)){n+=4;x<<=4;}
 if(!(x&0xc0000000u)){n+=2;x<<=2;}
 if(!(x&0x80000000u))++n;return n;
}
static unsigned dos_bits64(unsigned long long x){
 unsigned hi=(unsigned)(x>>32);return hi?64u-(unsigned)__clzsi2(hi):32u-(unsigned)__clzsi2((unsigned)x);
}
static unsigned long long dos_divmod64(unsigned long long n,unsigned long long d,unsigned long long* remainder){
 if(!d)abort();
 if(n<d){if(remainder)*remainder=n;return 0;}
 if(!(n>>32)){unsigned a=(unsigned)n,b=(unsigned)d,q=a/b;if(remainder)*remainder=a-q*b;return q;}
 if(!(d&(d-1))){if(remainder)*remainder=n&(d-1);return n>>(dos_bits64(d)-1);}
 unsigned shift=dos_bits64(n)-dos_bits64(d);
 unsigned long long step=d<<shift,bit=1ULL<<shift,q=0;
 do{if(n>=step){n-=step;q|=bit;}step>>=1;bit>>=1;}while(bit);
 if(remainder)*remainder=n;return q;
}
unsigned long long __udivdi3(unsigned long long n,unsigned long long d){return dos_divmod64(n,d,NULL);}
unsigned long long __umoddi3(unsigned long long n,unsigned long long d){unsigned long long r;dos_divmod64(n,d,&r);return r;}
#else
int __clzsi2(unsigned x){int n=32;while(x){--n;x>>=1;}return n;}
unsigned long long __udivdi3(unsigned long long n,unsigned long long d){if(!d)abort();unsigned long long q=0,r=0;for(int i=63;i>=0;--i){unsigned top=(unsigned)(r>>63);r=(r<<1)|((n>>i)&1);if(top||r>=d){r-=d;q|=1ULL<<i;}}return q;}
unsigned long long __umoddi3(unsigned long long a,unsigned long long b){return a-__udivdi3(a,b)*b;}
#endif
long long __divdi3(long long a,long long b){unsigned long long x=a<0?0ULL-(unsigned long long)a:(unsigned long long)a,y=b<0?0ULL-(unsigned long long)b:(unsigned long long)b;unsigned long long q=__udivdi3(x,y);return (a<0)^(b<0)?0ULL-q:q;}
long long __moddi3(long long a,long long b){return a-__divdi3(a,b)*b;}
/* DOS_DIVISION_PRIMITIVES_END */
#endif
int abs(int x){return x<0?-x:x;}
void __assert_func(const char* file,int line,const char* func,const char* expr){(void)file;(void)line;(void)func;(void)expr;abort();}
int strncasecmp(const char* a,const char* b,size_t n){while(n--){unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y||!x)return (int)x-(int)y;}return 0;}
static uint32_t random_state=1;
int rand(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state&0x7fffffff;}
void srand(unsigned seed){random_state=seed?seed:1;}
static void byte_swap(unsigned char* a,unsigned char* b,size_t n){while(n--){unsigned char v=*a;*a++=*b;*b++=v;}}
void qsort(void* data,size_t n,size_t size,int(*cmp)(const void*,const void*)){
 if(n<2||!size||n>SIZE_MAX/size)return;unsigned char* a=data;
 for(size_t start=n/2,end=n;end>1;){size_t root;if(start)root=--start;else{byte_swap(a,a+(--end)*size,size);root=0;}
  while(root<(end/2)){size_t child=root*2+1;if(child+1<end&&cmp(a+child*size,a+(child+1)*size)<0)++child;if(cmp(a+root*size,a+child*size)>=0)break;byte_swap(a+root*size,a+child*size,size);root=child;}
 }
}
#include <time.h>
time_t time(time_t* result){const struct tab5_app_api4* api=(const void*)tab5_api;time_t value=tab5_api&&tab5_api->base.version>=4&&api->unix_time?api->unix_time():0;if(result)*result=value;return value;}

static struct tm calendar_result;
struct tm* localtime(const time_t* value){if(!value||!tab5_api||tab5_api->base.version<4)return NULL;int fields[9];const struct tab5_app_api4* api=(const void*)tab5_api;if(!api->calendar||api->calendar((uint32_t)*value,0,fields))return NULL;memset(&calendar_result,0,sizeof(calendar_result));calendar_result.tm_sec=fields[0];calendar_result.tm_min=fields[1];calendar_result.tm_hour=fields[2];calendar_result.tm_mday=fields[3];calendar_result.tm_mon=fields[4];calendar_result.tm_year=fields[5];calendar_result.tm_wday=fields[6];calendar_result.tm_yday=fields[7];calendar_result.tm_isdst=fields[8];return &calendar_result;}
