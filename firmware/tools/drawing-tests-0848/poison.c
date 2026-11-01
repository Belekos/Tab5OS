#include "image_core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sanitizer/asan_interface.h>
static unsigned checks,allocations;static size_t live;static uint8_t *region;static size_t bytes;
#define C(x) do{++checks;assert(x);}while(0)
void *__real_memcpy(void*,const void*,size_t);
void *__wrap_memcpy(void*d,const void*s,size_t n){uintptr_t p=(uintptr_t)d,r=(uintptr_t)region;if(region&&p>=r&&p-r<bytes){C(n<=bytes-(p-r));__asan_unpoison_memory_region(d,n);}return __real_memcpy(d,s,n);}
static void*alloc(void*c,size_t n){(void)c;++allocations;size_t*p=malloc(n+sizeof(size_t));C(p);*p=n;live+=n;memset(p+1,0xa5,n);return p+1;}
static void release(void*c,void*p){(void)c;if(p){size_t*b=(size_t*)p-1;__asan_unpoison_memory_region(p,*b);live-=*b;free(b);}}
static void test(int w,int h,int warp,int cancel){IEDoc d;C(ie_init(&d,alloc,release,0,w,h));size_t n=(size_t)w*h;uint32_t *initial=malloc(n*4),*after=malloc(n*4);C(initial&&after);for(size_t i=0;i<n;i++)d.current.layers[0].pixels[i]=(uint32_t)(i*12347u)^0xaabbbbaau;memcpy(initial,d.current.layers[0].pixels,n*4);
 C(warp?ie_warp_begin(&d,31):ie_brush_begin(&d));IEState*s=d.undo[d.undo_count-1];C(s->gesture_blocks);region=(uint8_t*)s->layers[0].pixels;bytes=n*4;__asan_poison_memory_region(region,bytes);unsigned calls=allocations;
 if(warp)C(ie_warp_brush(&d,w/3,h/3,w/2,h/2,31,80,IE_WARP_PINCH));else{IEBrush b={0xffffaabb,7,48,40,25,0,0};C(ie_brush(&d,w/2,h/2,w/2,h/2,&b));C(ie_brush(&d,w/2,h/2,w/2+1,h/2+1,&b));}
 C(allocations==calls);C(!d.current.gesture_blocks);memcpy(after,d.current.layers[0].pixels,n*4);
 if(cancel){C(ie_cancel(&d));region=0;C(!memcmp(initial,d.current.layers[0].pixels,n*4));}
 else{ie_commit(&d);C(ie_undo(&d));C(!memcmp(initial,d.current.layers[0].pixels,n*4));C(!d.current.gesture_blocks);C(ie_redo(&d));C(!memcmp(after,d.current.layers[0].pixels,n*4));C(ie_undo(&d));C(!memcmp(initial,d.current.layers[0].pixels,n*4));}
 region=0;ie_destroy(&d);free(initial);free(after);C(!live);
}
int main(void){int widths[]={1,257,641,1280},heights[]={1,3,359,720};for(unsigned i=0;i<4;i++)for(int warp=0;warp<2;warp++)for(int cancel=0;cancel<2;cancel++)test(widths[i],heights[i],warp,cancel);printf("{\"passed\":true,\"checks\":%u,\"outstanding\":%zu,\"allocations\":%u}\n",checks,live,allocations);}
