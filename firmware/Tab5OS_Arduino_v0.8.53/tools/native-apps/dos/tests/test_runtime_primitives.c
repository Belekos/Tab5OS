/* Test the extracted production primitives against the host C implementation.
 * Separate guarded mappings put the last valid byte at an inaccessible page.
 * This catches wide overreads/overwrites that ordinary array tests miss. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <setjmp.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static jmp_buf abort_target;
static void runtime_abort(void){longjmp(abort_target,1);}
#define abort runtime_abort
#define memset tested_memset
#define memcpy tested_memcpy
#define memmove tested_memmove
#define __clzsi2 tested_clz
#define __udivdi3 tested_udiv
#define __umoddi3 tested_umod
#define __divdi3 tested_div
#define __moddi3 tested_mod
#include "runtime_memory.inc"
#include "runtime_division.inc"
#undef abort
#undef memset
#undef memcpy
#undef memmove
#undef __clzsi2
#undef __udivdi3
#undef __umoddi3
#undef __divdi3
#undef __moddi3

struct block {size_t size;struct block* next;int used;};
static struct block* heap;
static size_t heapCapacity;
#include "runtime_heap_stats.inc"

static unsigned long long checks,memory_cases,division_cases;
static void check(int ok,const char* reason){++checks;if(!ok){fprintf(stderr,"FAILED: %s\n",reason);exit(1);}}
static uint64_t rng=UINT64_C(0x96a47b13f24e7921);
static uint64_t random64(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
static void pattern(unsigned char* p,size_t n){for(size_t i=0;i<n;i++)p[i]=(unsigned char)(i*31u+i/7u+0x53u);}

static void test_memory(void){
 unsigned char src[640],got[640],expected[640];pattern(src,sizeof(src));
 for(size_t a=0;a<16;a++)for(size_t b=0;b<16;b++)for(size_t n=0;n<=257;n++){
  memset(got,0xa5,sizeof(got));memset(expected,0xa5,sizeof(expected));
  memcpy(expected+32+a,src+32+b,n);
  check(tested_memcpy(got+32+a,src+32+b,n)==got+32+a,"memcpy return");
  check(!memcmp(got,expected,sizeof(got)),"memcpy alignment/tail/redzones");++memory_cases;
 }
 for(size_t a=0;a<16;a++)for(size_t n=0;n<=257;n++)for(int value=-1;value<=256;value+=257){
  pattern(got,sizeof(got));memcpy(expected,got,sizeof(got));
  memset(expected+32+a,value,n);
  check(tested_memset(got+32+a,value,n)==got+32+a,"memset return");
  check(!memcmp(got,expected,sizeof(got)),"memset alignment/tail/redzones");++memory_cases;
 }
 for(size_t a=0;a<48;a++)for(size_t b=0;b<48;b++)for(size_t n=0;n<=257;n+=n<32?1:17){
  pattern(got,sizeof(got));memcpy(expected,got,sizeof(got));
  memmove(expected+32+a,expected+32+b,n);
  check(tested_memmove(got+32+a,got+32+b,n)==got+32+a,"memmove return");
  check(!memcmp(got,expected,sizeof(got)),"memmove overlap/alignment/redzones");++memory_cases;
 }
 size_t canvas=800u*480u*2u;
 unsigned char* large=malloc(canvas+64),*copy=malloc(canvas+64);
 check(large&&copy,"framebuffer allocation");
 pattern(large,canvas+64);memset(copy,0x39,canvas+64);
 tested_memcpy(copy+32,large+32,canvas);
 check(!memcmp(copy+32,large+32,canvas),"complete framebuffer copy");
 for(size_t i=0;i<32;i++)check(copy[i]==0x39&&copy[canvas+32+i]==0x39,"framebuffer redzones");
 tested_memset(copy+32,0xb7,canvas);
 for(size_t i=0;i<canvas;i++)if(copy[32+i]!=0xb7)check(0,"complete framebuffer set");
 check(1,"complete framebuffer set");free(large);free(copy);
}

struct guarded {unsigned char* start;size_t page;};
static struct guarded guard_alloc(void){
 struct guarded g;
#ifdef _WIN32
 SYSTEM_INFO si;GetSystemInfo(&si);g.page=si.dwPageSize;
 g.start=VirtualAlloc(NULL,g.page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 check(g.start!=NULL,"VirtualAlloc guard");DWORD old;
 check(VirtualProtect(g.start+g.page,g.page,PAGE_NOACCESS,&old)!=0,"VirtualProtect guard");
#else
 g.page=(size_t)sysconf(_SC_PAGESIZE);
 g.start=mmap(NULL,g.page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 check(g.start!=MAP_FAILED,"mmap guard");
 check(mprotect(g.start+g.page,g.page,PROT_NONE)==0,"mprotect guard");
#endif
 return g;
}
static void guard_free(struct guarded g){
#ifdef _WIN32
 check(VirtualFree(g.start,0,MEM_RELEASE)!=0,"VirtualFree guard");
#else
 check(munmap(g.start,g.page*2)==0,"munmap guard");
#endif
}
static void test_guards(void){
 struct guarded a=guard_alloc(),b=guard_alloc();
 for(size_t n=0;n<=257;n++){
  unsigned char* x=a.start+a.page-n,*y=b.start+b.page-n;
  pattern(x,n);tested_memcpy(y,x,n);check(!memcmp(x,y,n),"guard-page copy end");
  tested_memset(y,0x5b,n);for(size_t i=0;i<n;i++)check(y[i]==0x5b,"guard-page set end");
  for(size_t gap=1;gap<=16;gap++){
   pattern(x-gap,n+gap);unsigned char expected[274];memcpy(expected,x-gap,n+gap);
   memmove(expected+gap,expected,n);tested_memmove(x,x-gap,n);
   check(!memcmp(x-gap,expected,n+gap),"guard-page backward move");
   pattern(x-gap,n+gap);memcpy(expected,x-gap,n+gap);
   memmove(expected,expected+gap,n);tested_memmove(x-gap,x,n);
   check(!memcmp(x-gap,expected,n+gap),"guard-page forward move");
  }
 }
 guard_free(a);guard_free(b);
}

static void division(uint64_t a,uint64_t b){
 if(!b)return;
 check(tested_udiv(a,b)==a/b,"unsigned division");
 check(tested_umod(a,b)==a%b,"unsigned remainder");
 unsigned long long rem=~0ULL,q=dos_divmod64(a,b,&rem);
 check(q==a/b&&rem==a%b,"combined quotient/remainder");++division_cases;
}
static void signed_division(int64_t a,int64_t b){
 if(!b||(a==INT64_MIN&&b==-1))return;
 check(tested_div(a,b)==a/b,"signed division");
 check(tested_mod(a,b)==a%b,"signed remainder");++division_cases;
}
static void test_division(void){
 static const uint64_t edge[]={0,1,2,3,7,15,16,31,32,63,64,255,256,65535,65536,65537,
  UINT64_C(0x7fffffff),UINT64_C(0x80000000),UINT64_C(0xffffffff),UINT64_C(0x100000000),
  UINT64_C(0x100000001),UINT64_C(0x7fffffffffffffff),UINT64_C(0x8000000000000000),UINT64_MAX};
 for(size_t i=0;i<sizeof(edge)/sizeof(edge[0]);i++)for(size_t j=0;j<sizeof(edge)/sizeof(edge[0]);j++){
  division(edge[i],edge[j]);signed_division((int64_t)edge[i],(int64_t)edge[j]);
 }
 for(unsigned bit=0;bit<64;bit++)for(int delta=-1;delta<=1;delta++){
  uint64_t d=(UINT64_C(1)<<bit)+(uint64_t)delta;
  division(UINT64_MAX,d);division(d,d);division(d+1,d);division(d-1,d);
 }
 for(unsigned i=0;i<200000;i++){
  uint64_t a=random64(),b=random64();division(a,b);
  division((uint32_t)a,(uint32_t)b);division(a,(uint16_t)b);
  signed_division((int64_t)a,(int64_t)b);
  unsigned x=(unsigned)a,count=0;for(unsigned mask=0x80000000u;mask&&!(x&mask);mask>>=1)++count;
  check(tested_clz(x)==(int)count,"leading zeros");
 }
 check(tested_clz(0)==32,"leading zeros zero");
 for(unsigned bit=0;bit<32;bit++)check(tested_clz(1u<<bit)==31-(int)bit,"leading zeros powers");
 if(!setjmp(abort_target)){tested_udiv(1,0);check(0,"zero divisor must abort");}
 else check(1,"zero divisor abort");
 if(!setjmp(abort_target)){tested_umod(0,0);check(0,"zero remainder divisor must abort");}
 else check(1,"zero remainder divisor abort");
}

static void test_heap_stats(void){
 size_t capacity,available,largest;
 heapCapacity=8u*1024u*1024u;heap=NULL;
 tab5_runtime_memory(&capacity,&available,&largest);
 check(capacity==heapCapacity&&available==heapCapacity-sizeof(struct block)&&largest==available,"unallocated heap budget excludes header");
 check(heap==NULL,"statistics must not allocate the arena");
 struct block blocks[4]={{2048,NULL,0},{4096,NULL,1},{1024,NULL,0},{8192,NULL,0}};
 for(unsigned i=0;i<3;i++)blocks[i].next=&blocks[i+1];
 heap=blocks;tab5_runtime_memory(&capacity,&available,&largest);
 check(capacity==heapCapacity&&available==11264&&largest==8192,"fragmented free bytes and largest block exclude allocations");
 for(unsigned i=0;i<4;i++)blocks[i].used=1;
 tab5_runtime_memory(NULL,&available,&largest);
 check(available==0&&largest==0,"full heap has no free block");
 blocks[2].used=0;tab5_runtime_memory(NULL,NULL,&largest);
 check(largest==1024,"individual statistic query");
 tab5_runtime_memory(NULL,NULL,NULL);heap=NULL;
}

int main(void){
 test_memory();test_guards();test_division();test_heap_stats();
 printf("%llu checks passed; %llu memory cases; %llu division cases; inaccessible guard pages; zero-divisor aborts\n",checks,memory_cases,division_cases);
 return 0;
}
