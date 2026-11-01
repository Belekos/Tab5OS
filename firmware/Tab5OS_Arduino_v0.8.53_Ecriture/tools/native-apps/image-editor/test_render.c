#define _POSIX_C_SOURCE 200809L
#include "image_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <time.h>
static unsigned long checks;
static size_t live,peak,calls;
#define CHECK(v) do{checks++;if(!(v)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v);exit(1);}}while(0)
typedef union {size_t bytes;long double align;} Header;
static void*get_mem(void*ctx,size_t n){Header*h;(void)ctx;h=malloc(sizeof(*h)+n);CHECK(h!=0);h->bytes=n;live+=n;calls++;if(live>peak)peak=live;return h+1;}
static void free_mem(void*ctx,void*p){Header*h;(void)ctx;if(!p)return;h=(Header*)p-1;CHECK(live>=h->bytes);live-=h->bytes;free(h);}
static uint32_t random32(uint32_t*x){*x^=*x<<13;*x^=*x>>17;*x^=*x<<5;return *x;}
/* Independent reference: the unoptimized 0.8.32 straight-alpha equation. */
static uint32_t reference_over(uint32_t dst,uint32_t src,unsigned opacity){unsigned sa=((src>>24)*opacity+127)/255,da=dst>>24,oa=sa+(da*(255-sa)+127)/255;uint32_t o=oa<<24;unsigned k;if(!oa)return 0;for(k=0;k<24;k+=8){unsigned sc=(src>>k)&255,dc=(dst>>k)&255;unsigned c=(sc*sa+(dc*da*(255-sa)+127)/255+oa/2)/oa;if(c>255)c=255;o|=c<<k;}return o;}
static uint32_t reference_composite(const IEDoc*d,int x,int y){uint32_t out=0;int l;for(l=0;l<d->current.count;l++){const IELayer*p=&d->current.layers[l];if(p->visible)out=reference_over(out,p->pixels[(size_t)y*d->current.width+x],p->opacity);}return out;}
static uint16_t flatten(uint32_t c,int x,int y,uint32_t light,uint32_t dark){uint32_t bg=((x/8+y/8)&1)?dark:light;unsigned a=c>>24,r=((c&255)*a+((bg>>16)&255)*(255-a))/255,g=(((c>>8)&255)*a+((bg>>8)&255)*(255-a))/255,b=(((c>>16)&255)*a+(bg&255)*(255-a))/255;return(uint16_t)(((r&248)<<8)|((g&252)<<3)|(b>>3));}
static void fixture(IEDoc*d,int w,int h,int count){uint32_t rng=42;int l;size_t i,n=(size_t)w*h;CHECK(ie_init(d,get_mem,free_mem,0,w,h));for(l=0;l<count;l++){IELayer*p=&d->current.layers[l];if(l)p->pixels=get_mem(0,n*sizeof(uint32_t));p->opacity=(uint8_t)(l%3==0?255:l%3==1?127:0);p->visible=l%4!=3;for(i=0;i<n;i++){uint32_t a=random32(&rng);if(i%4==0)a|=0xff000000u;if(i%4==1)a&=0xffffffu;p->pixels[i]=a;}}d->current.count=count;d->current.active=count-1;}
static uint64_t hash_doc(const IEDoc*d){uint64_t h=1469598103934665603ull;int l;size_t i,n=(size_t)d->current.width*d->current.height;for(l=0;l<d->current.count;l++)for(i=0;i<n;i++){h^=d->current.layers[l].pixels[i];h*=1099511628211ull;}h^=d->random;return h;}
#ifdef IE_BASELINE
static size_t reference_render(const IEDoc*d,uint16_t*out,size_t stride,int ow,int oh,int x,int y,int w,int h,uint32_t light,uint32_t dark){int xx,yy;size_t n=0;for(yy=0;yy<oh;yy++)for(xx=0;xx<ow;xx++){int sx=xx*d->current.width/ow,sy=yy*d->current.height/oh;if(sx>=x&&sy>=y&&(int64_t)sx<(int64_t)x+w&&(int64_t)sy<(int64_t)y+h){out[(size_t)yy*stride+xx]=flatten(ie_composite(d,sx,sy),sx,sy,light,dark);n++;}}return n;}
#define RENDER reference_render
#else
#define RENDER ie_render_scaled_rgb565
#endif
static uint64_t test_pixels(void){IEDoc d;uint16_t out[130*107];int dimensions[][2]={{61,47},{29,23},{1,1},{120,101},{60,46},{17,39}};int regions[][4]={{0,0,61,47},{1,1,1,1},{9,9,17,13},{0,0,1,47},{60,0,1,47},{0,46,61,1},{-5,-7,10,10},{-1,-1,INT_MAX,INT_MAX},{INT_MIN,0,INT_MAX,1},{INT_MAX,INT_MAX,INT_MAX,INT_MAX},{0,0,0,2},{4,4,-2,2},{61,47,1,1}};unsigned a,b;uint64_t hash;size_t before;fixture(&d,61,47,8);hash=hash_doc(&d);before=calls;ie_select(&d,5,5,9,9);/* Selection does not crop the displayed image. */
for(a=0;a<sizeof(dimensions)/sizeof(dimensions[0]);a++)for(b=0;b<sizeof(regions)/sizeof(regions[0]);b++){int ow=dimensions[a][0],oh=dimensions[a][1],*r=regions[b],xx,yy;size_t expected=0,written,i;for(i=0;i<sizeof(out)/sizeof(out[0]);i++)out[i]=0x5a5a;written=RENDER(&d,out,130,ow,oh,r[0],r[1],r[2],r[3],0xe0e9f1,0x454954);for(yy=0;yy<107;yy++)for(xx=0;xx<130;xx++){int sx=xx*d.current.width/ow,sy=yy*d.current.height/oh;int inside=xx<ow&&yy<oh&&r[2]>0&&r[3]>0&&sx>=r[0]&&sy>=r[1]&&(int64_t)sx<(int64_t)r[0]+r[2]&&(int64_t)sy<(int64_t)r[1]+r[3];if(inside){uint32_t c=reference_composite(&d,sx,sy);CHECK(ie_composite(&d,sx,sy)==c);CHECK(out[yy*130+xx]==flatten(c,sx,sy,0xe0e9f1,0x454954));expected++;}else CHECK(out[yy*130+xx]==0x5a5a);}CHECK(written==expected);}
#ifndef IE_BASELINE
CHECK(ie_render_rgb565(&d,out,130,0,0,61,47,0xffffff,0)==61u*47u);
CHECK(ie_render_scaled_rgb565(0,out,130,10,10,0,0,10,10,0,0)==0);
CHECK(ie_render_scaled_rgb565(&d,0,130,10,10,0,0,10,10,0,0)==0);
CHECK(ie_render_scaled_rgb565(&d,out,9,10,10,0,0,10,10,0,0)==0);
CHECK(ie_render_scaled_rgb565(&d,out,SIZE_MAX,10,10,0,0,10,10,0,0)==0);
CHECK(ie_render_scaled_rgb565(&d,out,130,0,10,0,0,10,10,0,0)==0);
CHECK(ie_render_scaled_rgb565(&d,out,130,10,1025,0,0,10,10,0,0)==0);
#endif
CHECK(calls==before);CHECK(hash_doc(&d)==hash);ie_destroy(&d);CHECK(live==0);return hash;}
static uint64_t test_brush(void){IEDoc d;uint64_t hash;unsigned size,square,hard,diff;CHECK(ie_init(&d,get_mem,free_mem,0,90,70));CHECK(ie_new(&d,90,70,0));for(size=1;size<=256;size=size<16?size+1:size*2)for(square=0;square<2;square++)for(hard=0;hard<=100;hard=hard==0?50:hard==50?85:hard==85?100:101)for(diff=0;diff<=100;diff+=50){IEBrush b={IE_RGBA(size,hard,diff,255),size,73,hard,diff,square,diff==50};ie_brush(&d,3,2,13,12,&b);ie_select(&d,12,11,66,48);ie_brush(&d,80,63,72,54,&b);ie_deselect(&d);}hash=hash_doc(&d);ie_destroy(&d);CHECK(live==0);return hash;}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
static void bench(double*full,double*small,double*brush,double*soft){IEDoc d;uint16_t*out;int i;double start;fixture(&d,640,360,8);out=malloc(640u*360u*2u);CHECK(out!=0);start=now();for(i=0;i<20;i++)RENDER(&d,out,640,640,360,0,0,640,360,0xffffff,0xcccccc);*full=(now()-start)/20.0;start=now();for(i=0;i<200;i++)RENDER(&d,out,640,640,360,90,90,24,24,0xffffff,0xcccccc);*small=(now()-start)/200.0;ie_destroy(&d);CHECK(ie_init(&d,get_mem,free_mem,0,640,360));start=now();for(i=0;i<5;i++){IEBrush b={0xff0033eeu,128,100,100,0,0,0};ie_brush(&d,150,120,350,120,&b);}*brush=(now()-start)/5.0;start=now();for(i=0;i<5;i++){IEBrush b={0xff0033eeu,128,100,85,0,0,0};ie_brush(&d,150,120,350,120,&b);}*soft=(now()-start)/5.0;free(out);ie_destroy(&d);CHECK(live==0);}
int main(int argc,char**argv){uint64_t composite=test_pixels(),stroke=test_brush();double full=0,small=0,brush=0,soft=0;(void)argv;if(argc>1)bench(&full,&small,&brush,&soft);printf("{\"status\":\"passed\",\"checks\":%lu,\"liveBytes\":%zu,\"peakBytes\":%zu,\"compositeDigest\":\"%016llx\",\"brushDigest\":\"%016llx\",\"fullSeconds\":%.9f,\"regionSeconds\":%.9f,\"brushSeconds\":%.9f,\"softBrushSeconds\":%.9f}\n",checks,live,peak,(unsigned long long)composite,(unsigned long long)stroke,full,small,brush,soft);return 0;}
