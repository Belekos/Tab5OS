#include "image_core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static size_t checks,live,peak,copies,zeros;static int fail_after=-1;static int baseline;
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"check failed line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void *__real_memcpy(void*,const void*,size_t);void *__real_memset(void*,int,size_t);
void *__wrap_memcpy(void*d,const void*s,size_t n){copies+=n;return __real_memcpy(d,s,n);}
void *__wrap_memset(void*d,int c,size_t n){zeros+=n;return __real_memset(d,c,n);}
static void*alloc(void*c,size_t n){(void)c;if(fail_after==0)return 0;if(fail_after>0)--fail_after;size_t*p=malloc(n+sizeof(size_t));if(!p)return 0;*p=n;live+=n;if(live>peak)peak=live;__real_memset(p+1,0xa5,n);return p+1;}
static void release(void*c,void*p){(void)c;if(!p)return;size_t*b=(size_t*)p-1;C(live>=*b);live-=*b;free(b);}
static uint64_t hash(const IEDoc*d){uint64_t h=1469598103934665603ull;int l,x,y;const IEState*s=&d->current;
#define H(v) do{h^=(uint64_t)(v);h*=1099511628211ull;}while(0)
H(s->width);H(s->height);H(s->count);H(s->active);H(s->selection.enabled);H(s->selection.x);H(s->selection.y);H(s->selection.w);H(s->selection.h);
for(l=0;l<s->count;l++){C(s->layers[l].pixels!=0);H(s->layers[l].opacity);H(s->layers[l].visible);for(x=0;x<24;x++)H(s->layers[l].name[x]);for(x=0;x<s->width*s->height;x++)H(s->layers[l].pixels[x]);}
for(y=0;y<s->height;y++)for(x=0;x<s->width;x++)H(ie_selection_alpha(s,x,y));
return h;
#undef H
}
static void setup(IEDoc*d,int w,int h,int layers){C(ie_init(d,alloc,release,0,w,h));for(int l=1;l<layers;l++)C(ie_layer_add(d,"Layer"));for(int l=0;l<layers;l++)for(int i=0;i<w*h;i++)d->current.layers[l].pixels[i]=IE_RGBA((i*3+l*41)%256,(i*7+l*29)%256,(i*11+l*71)%256,100+(i+l)%156);d->current.active=layers-1;}
static void done(IEDoc*d){ie_destroy(d);C(live==0);}
static void brush(IEDoc*d,int seed){IEBrush b={IE_RGBA(31+seed,99,211,255),17,43,51,seed%61,seed%6,seed&1};C(ie_brush(d,1,2,d->current.width-2,3,&b));C(ie_brush(d,d->current.width-2,3,2,d->current.height-2,&b));C(ie_brush(d,2,d->current.height-2,d->current.width-1,d->current.height-1,&b));}
static void selection(IEDoc*d){IESelectionOptions q={0};q.method=IE_SELECT_ELLIPSE;q.combine=IE_SELECT_REPLACE;q.x0=1;q.y0=1;q.x1=d->current.width-2;q.y1=d->current.height-2;q.feather=-3;IEJob*j=ie_selection_start(d,&q);C(j!=0);int r;do{r=ie_job_step(j,31);}while(!r);C(r>0);ie_job_destroy(j);}
static void small(void){IEDoc d;setup(&d,37,29,3);selection(&d);for(int seed=0;seed<24;seed++){d.current.active=seed%3;uint64_t before=hash(&d);uint32_t*other=d.current.layers[(seed+1)%3].pixels;C(ie_brush_begin(&d));size_t captured=0;for(int l=0;l<3;l++)captured+=d.undo[d.undo_count-1]->layers[l].pixels!=0;C(captured==1);brush(&d,seed);C(d.current.layers[(seed+1)%3].pixels==other);uint64_t after=hash(&d);C(before!=after);ie_commit(&d);C(ie_undo(&d));C(hash(&d)==before);C(ie_redo(&d));C(hash(&d)==after);
C(ie_layer_delete(&d));uint64_t deleted=hash(&d);C(ie_undo(&d)&&hash(&d)==after);C(ie_undo(&d)&&hash(&d)==before);C(ie_redo(&d)&&hash(&d)==after);C(ie_redo(&d)&&hash(&d)==deleted);C(ie_undo(&d)&&hash(&d)==after);
C(ie_brush_begin(&d));brush(&d,seed+3);C(ie_cancel(&d)&&hash(&d)==after);C(ie_redo(&d)&&hash(&d)==deleted);C(ie_undo(&d)&&hash(&d)==after);
C(ie_warp_begin(&d,25));C(ie_warp_brush(&d,10,10,17,18,25,80,IE_WARP_PUSH));C(ie_cancel(&d)&&hash(&d)==after);
C(ie_brush_begin(&d));brush(&d,seed+7);uint64_t painted=hash(&d);ie_commit(&d);C(ie_resize(&d,41,31));uint64_t resized=hash(&d);C(ie_undo(&d)&&hash(&d)==painted);C(ie_undo(&d)&&hash(&d)==after);C(ie_redo(&d)&&hash(&d)==painted);C(ie_redo(&d)&&hash(&d)==resized);C(ie_undo(&d)&&hash(&d)==painted);
}printf("trace=%016llx\n",(unsigned long long)hash(&d));done(&d);}
static void failures(void){for(int mode=0;mode<2;mode++)for(int at=0;at<12;at++){IEDoc d;setup(&d,32,24,4);selection(&d);uint64_t before=hash(&d);fail_after=at;int ok=mode?ie_warp_begin(&d,48):ie_brush_begin(&d);fail_after=-1;if(ok){if(mode)C(ie_warp_brush(&d,7,8,17,9,48,70,IE_WARP_PINCH));else brush(&d,8);C(ie_cancel(&d));}C(hash(&d)==before);done(&d);}}
static void metrics(int layers){IEDoc d;setup(&d,1280,720,layers);copies=zeros=0;size_t resident=live;C(ie_brush_begin(&d));size_t beginCopy=copies,beginZero=zeros,allocated=live-resident;IEBrush b={IE_RGBA(0,0,0,255),2,100,100,0,0,0};copies=zeros=0;C(ie_brush(&d,120,100,120,100,&b));printf("metric layers=%d copy=%zu initwrites=%zu firststampzero=%zu firststampcopy=%zu allocated=%zu history=%zu\n",layers,beginCopy,beginZero,zeros,copies,allocated,d.history_bytes);C(ie_cancel(&d));done(&d);}
int main(int argc,char**argv){baseline=argc>1&&!strcmp(argv[1],"--baseline");small();failures();metrics(1);metrics(2);printf("{\"passed\":true,\"checks\":%zu,\"peak\":%zu,\"outstanding\":%zu}\n",checks,peak,live);return 0;}
