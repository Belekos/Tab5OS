#include "image_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Regression tests execute the production fill job, including its allocator,
   compositing, undo and cancellation paths. No display or device is required. */
static unsigned checks;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v); exit(1); } } while(0)
typedef struct { size_t live,calls; long fail_at; } Memory;
typedef union { size_t bytes; long double alignment; } Header;
static void* allocate(void* context,size_t bytes){
 Memory* m=context;Header* h;size_t call=m->calls++;
 if(m->fail_at>=0&&call>=(size_t)m->fail_at)return 0;
 h=malloc(sizeof(*h)+bytes);if(!h)return 0;h->bytes=bytes;m->live+=bytes;return h+1;
}
static void release(void* context,void* pointer){
 Memory* m=context;Header* h;if(!pointer)return;h=(Header*)pointer-1;
 CHECK(h->bytes<=m->live);m->live-=h->bytes;free(h);
}
static void setup(IEDoc* d,Memory* m,uint32_t color){
 memset(m,0,sizeof(*m));m->fail_at=-1;CHECK(ie_init(d,allocate,release,m,40,36));CHECK(ie_new(d,40,36,color));
}
static void cleanup(IEDoc* d,Memory* m){ie_destroy(d);CHECK(m->live==0);}
static uint64_t pixels(const IEDoc* d){
 size_t i,n=(size_t)d->current.width*d->current.height;uint64_t result=1469598103934665603ull;
 for(i=0;i<n;i++){result^=d->current.layers[d->current.active].pixels[i];result*=1099511628211ull;}return result;
}
static int finish(IEJob* job){int result=0;unsigned steps=0;CHECK(job!=0);while(!result){result=ie_job_step(job,37);CHECK(++steps<20000);}ie_job_destroy(job);return result;}
static IEFillOptions options(void){
 IEFillOptions q;memset(&q,0,sizeof(q));q.kind=IE_FILL_RELIEF;q.region=IE_FILL_SELECTION;q.x=20;q.y=18;q.opacity=100;q.density=100;q.scale=12;
 q.foreground=IE_RGBA(220,40,20,255);q.background=IE_RGBA(20,40,220,255);
 q.relief_profile=IE_RELIEF_CIRCULAR;q.relief_unit=IE_RELIEF_PERCENT;q.relief_extent=100;q.relief_height=16;q.relief_azimuth=225;q.relief_intensity=100;return q;
}
static void test_selected_colors(void){
 int profile;for(profile=IE_RELIEF_LINEAR;profile<=IE_RELIEF_EXTRUDE;profile++){
  IEDoc d;Memory m;IEFillOptions q=options();uint32_t first[40*36];uint64_t before,after;int x,y;
  setup(&d,&m,IE_RGBA(30,190,90,255));ie_select(&d,4,4,35,31);q.relief_profile=profile;before=pixels(&d);
  CHECK(finish(ie_fill_start(&d,&q))==1);after=pixels(&d);CHECK(after!=before);
  memcpy(first,d.current.layers[0].pixels,sizeof(first));
  if(profile==IE_RELIEF_EXTRUDE)CHECK(first[18*40+20]==q.foreground);
  CHECK(ie_undo(&d));CHECK(pixels(&d)==before);CHECK(ie_redo(&d));CHECK(pixels(&d)==after);CHECK(ie_undo(&d));
  q.foreground=IE_RGBA(20,40,220,255);q.background=IE_RGBA(220,40,20,255);CHECK(finish(ie_fill_start(&d,&q))==1);
  CHECK(pixels(&d)!=after);
  for(y=0;y<36;y++)for(x=0;x<40;x++){
   uint32_t a=first[y*40+x],b=d.current.layers[0].pixels[y*40+x];
   if(x<4||x>35||y<4||y>31){CHECK(a==IE_RGBA(30,190,90,255));CHECK(b==a);}
   else {CHECK((a&255)==((b>>16)&255));CHECK(((a>>16)&255)==(b&255));CHECK((a&0xff00)==(b&0xff00));CHECK((b>>24)==255);}
  }
  cleanup(&d,&m);
 }
}
static void soften_selection(IEDoc* d){
 IESelectionOptions q;memset(&q,0,sizeof(q));q.method=IE_SELECT_RECT;q.combine=IE_SELECT_REPLACE;q.x0=4;q.y0=4;q.x1=35;q.y1=31;q.feather=3;
 CHECK(finish(ie_selection_start(d,&q))==1);CHECK(d->current.selection_mask!=0);
}
static void test_flat_fill_and_transparency(void){
 unsigned mode,base;const uint32_t backgrounds[]={0,IE_RGBA(12,150,80,100),IE_RGBA(30,190,90,255)};
 for(base=0;base<3;base++)for(mode=0;mode<4;mode++){
  IEDoc d,e;Memory m,n;IEFillOptions q=options(),solid;uint64_t before;
  setup(&d,&m,backgrounds[base]);setup(&e,&n,backgrounds[base]);soften_selection(&d);soften_selection(&e);before=pixels(&d);
  q.foreground=IE_RGBA(200,30,80,mode&1?128:255);q.opacity=mode&1?43:100;
  if(mode&2)q.relief_height=0;else q.relief_intensity=0;
  solid=q;solid.kind=IE_FILL_SOLID;
  /* Flat relief must fit the same allocation allowance as an ordinary fill. */
  {size_t calls=n.calls;CHECK(finish(ie_fill_start(&e,&solid))==1);
   m.fail_at=(long)(m.calls+n.calls-calls);CHECK(finish(ie_fill_start(&d,&q))==1);m.fail_at=-1;}
  CHECK(pixels(&d)==pixels(&e));CHECK(pixels(&d)!=before);
  CHECK(d.current.layers[0].pixels[18*40+20]>>24);CHECK(d.current.layers[0].pixels[0]==backgrounds[base]);
  CHECK(ie_undo(&d));CHECK(pixels(&d)==before);cleanup(&d,&m);cleanup(&e,&n);
 }
 /* A shaded fill must also paint an empty layer, not only the flat special case. */
 {IEDoc d;Memory m;IEFillOptions q=options();setup(&d,&m,0);ie_select(&d,4,4,35,31);CHECK(finish(ie_fill_start(&d,&q))==1);CHECK((d.current.layers[0].pixels[18*40+20]>>24)==255);CHECK(d.current.layers[0].pixels[0]==0);cleanup(&d,&m);}
}
static void test_no_op_settings(void){
 int mode;for(mode=0;mode<2;mode++){
  IEDoc d;Memory m;IEFillOptions q=options();uint64_t before;setup(&d,&m,IE_RGBA(40,60,80,190));before=pixels(&d);
  if(mode)q.density=0;else q.opacity=0;
  CHECK(finish(ie_fill_start(&d,&q))==1);CHECK(pixels(&d)==before);CHECK(d.undo_count==0);cleanup(&d,&m);
 }
}
static void test_cancel(void){
 IEDoc d;Memory m;IEFillOptions q=options();IEJob* job;uint64_t before,redo;int dirty,result=0;unsigned step=0;
 setup(&d,&m,IE_RGBA(40,60,80,190));CHECK(ie_begin(&d));ie_shape(&d,IE_SHAPE_RECT,10,10,20,20,IE_RGBA(120,40,200,255),100,1,1,0);redo=pixels(&d);CHECK(ie_undo(&d));
 ie_select(&d,4,4,35,31);before=pixels(&d);dirty=d.dirty;CHECK(d.redo_count==1);job=ie_fill_start(&d,&q);CHECK(job!=0);
 while(!result&&pixels(&d)==before){result=ie_job_step(job,1);CHECK(++step<20000);}
 CHECK(result==0);CHECK(pixels(&d)!=before);CHECK(ie_job_progress(job)<100);
 ie_job_cancel(job);ie_job_destroy(job);CHECK(pixels(&d)==before);CHECK(d.dirty==dirty);CHECK(d.redo_count==1);CHECK(ie_redo(&d));CHECK(pixels(&d)==redo);cleanup(&d,&m);
}
static void test_allocation_failure(void){
 int fail;unsigned failed=0,succeeded=0;
 for(fail=0;fail<16;fail++){
  IEDoc d;Memory m;IEFillOptions q=options();IEJob* job;uint64_t before;int result=-1;
  setup(&d,&m,IE_RGBA(40,60,80,190));ie_select(&d,4,4,35,31);before=pixels(&d);m.fail_at=(long)m.calls+fail;
  job=ie_fill_start(&d,&q);if(job)result=finish(job);
  if(result<0){failed++;CHECK(pixels(&d)==before);}else {succeeded++;CHECK(pixels(&d)!=before);}
  m.fail_at=-1;cleanup(&d,&m);
 }
 CHECK(failed>=6);CHECK(succeeded>0);
}
int main(void){
 test_selected_colors();test_flat_fill_and_transparency();test_no_op_settings();test_cancel();test_allocation_failure();
 printf("{\"checks\":%u,\"allocation_scenarios\":16,\"status\":\"passed\"}\n",checks);return 0;
}
