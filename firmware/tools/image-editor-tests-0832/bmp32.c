/* BMP32 BI_RGB production decoder: row order, reserved alpha, hostile headers. */
#include "image_core.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
static size_t live,peak;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef union {size_t bytes;long double alignment;} Allocation;
static void* alloc_test(void* ctx,size_t n){(void)ctx;Allocation* a=malloc(sizeof(*a)+n);if(!a)return NULL;a->bytes=n;live+=n;if(live>peak)peak=live;return a+1;}
static void free_test(void* ctx,void* p){(void)ctx;if(p){Allocation* a=(Allocation*)p-1;CHECK(live>=a->bytes);live-=a->bytes;free(a);}}
typedef struct {const uint8_t* bytes;size_t length,offset,chunk;} Reader;
static size_t read_test(void* ctx,void* out,size_t n){Reader* r=ctx;CHECK(n<=4096);if(n>r->length-r->offset)n=r->length-r->offset;if(n>r->chunk)n=r->chunk;memcpy(out,r->bytes+r->offset,n);r->offset+=n;return n;}
static void put16(uint8_t* p,uint16_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void put32(uint8_t* p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
static void fixture(uint8_t p[70],int topdown){
 static const uint8_t top[8]={0,0,255,0,0,255,0,64};
 static const uint8_t bottom[8]={255,0,0,128,33,22,11,255};
 memset(p,0,70);p[0]='B';p[1]='M';put32(p+2,70);put32(p+10,54);put32(p+14,40);put32(p+18,2);put32(p+22,topdown?(uint32_t)-2:2);put16(p+26,1);put16(p+28,32);put32(p+34,16);
 memcpy(p+54,topdown?top:bottom,8);memcpy(p+62,topdown?bottom:top,8);
}
static uint64_t digest(const IEDoc* d){uint64_t h=1469598103934665603ULL;h^=(unsigned)d->current.width;h*=1099511628211ULL;h^=(unsigned)d->current.height;h*=1099511628211ULL;h^=(unsigned)d->current.count;for(int y=0;y<d->current.height;y++)for(int x=0;x<d->current.width;x++){h^=ie_composite(d,x,y);h*=1099511628211ULL;}return h;}
static void reject(IEDoc* d,const uint8_t* bytes,size_t n){uint64_t before=digest(d);Reader r={bytes,n,0,3};CHECK(!ie_load_bmp(d,read_test,&r));CHECK(digest(d)==before);CHECK(ie_error(d)[0]!=0);}
int main(void){
 IEDoc d;uint8_t good[70],bad[70];CHECK(ie_init(&d,alloc_test,free_test,NULL,3,3));
 for(int topdown=0;topdown<2;topdown++){
  fixture(good,topdown);Reader r={good,sizeof(good),0,3};CHECK(ie_load_bmp(&d,read_test,&r));CHECK(d.current.width==2&&d.current.height==2&&d.current.count==1);CHECK(r.offset==sizeof(good));
  CHECK(ie_composite(&d,0,0)==IE_RGBA(255,0,0,255));CHECK(ie_composite(&d,1,0)==IE_RGBA(0,255,0,255));CHECK(ie_composite(&d,0,1)==IE_RGBA(0,0,255,255));CHECK(ie_composite(&d,1,1)==IE_RGBA(11,22,33,255));
  /* Every reserved alpha byte, including zero, must produce opaque BI_RGB RGB. */
  for(int y=0;y<2;y++)for(int x=0;x<2;x++)CHECK((ie_composite(&d,x,y)>>24)==255);
  for(size_t n=0;n<sizeof(good);n++)reject(&d,good,n);
 }
 fixture(good,0);
 struct Mutation {unsigned offset,width;uint32_t value;} cases[]={
  {0,1,'X'},{14,4,12},{14,4,125},{10,4,53},{10,4,65537},
  {18,4,0},{18,4,1025},{18,4,0xffffffffu},{22,4,0},{22,4,1025},
  {22,4,(uint32_t)-1025},{22,4,0x80000000u},{26,2,0},{26,2,2},
  {28,2,16},{28,2,8},{30,4,1},{30,4,3},{30,4,0xffffffffu}
 };
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  memcpy(bad,good,sizeof(good));struct Mutation m=cases[i];if(m.width==4)put32(bad+m.offset,m.value);else if(m.width==2)put16(bad+m.offset,(uint16_t)m.value);else bad[m.offset]=(uint8_t)m.value;reject(&d,bad,sizeof(bad));
 }
 ie_destroy(&d);CHECK(live==0);
 printf("{\"checks\":%u,\"validFixtures\":2,\"truncationFixtures\":140,\"malformedHeaderFixtures\":19,\"peakAllocatedBytes\":%zu,\"remainingAllocatedBytes\":%zu}\n",checks,peak,live);
 return 0;
}
