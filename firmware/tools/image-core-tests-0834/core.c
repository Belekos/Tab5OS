#define main legacy_main
#include "test_core.c"
#undef main
#include <math.h>

static void test_opacity(void){
 IEDoc d;Memory m;IEBrush b={IE_RGBA(240,10,20,255),19,50,100,0,0,0};int i,shape;
 setup(&d,&m,64,64);CHECK(ie_new(&d,64,64,0));CHECK(ie_brush_begin(&d));
 for(i=0;i<12;i++)CHECK(ie_brush(&d,32,32,32,32,&b));
 CHECK((ie_composite(&d,32,32)>>24)==127);CHECK(d.brush_coverage!=0);ie_commit(&d);CHECK(d.brush_coverage==0);
 CHECK(ie_brush_begin(&d));CHECK(ie_brush(&d,32,32,32,32,&b));CHECK((ie_composite(&d,32,32)>>24)==191);CHECK(ie_cancel(&d));CHECK((ie_composite(&d,32,32)>>24)==127);
 for(shape=0;shape<6;shape++){uint64_t h;CHECK(ie_new(&d,64,64,0));b.square=shape;b.opacity=25;CHECK(ie_brush_begin(&d));CHECK(ie_brush(&d,16,16,45,41,&b));h=digest(&d);for(i=0;i<8;i++)CHECK(ie_brush(&d,16,16,45,41,&b));CHECK(digest(&d)==h);for(i=0;i<64*64;i++)CHECK((d.current.layers[0].pixels[i]>>24)<=63);ie_commit(&d);CHECK(ie_undo(&d));CHECK(!colored(&d));CHECK(ie_redo(&d));CHECK(digest(&d)==h);}
 CHECK(ie_new(&d,64,64,0xff665544));b.erase=1;b.opacity=50;b.square=0;CHECK(ie_brush_begin(&d));for(i=0;i<12;i++)CHECK(ie_brush(&d,32,32,32,32,&b));CHECK((ie_composite(&d,32,32)>>24)==128);CHECK(ie_cancel(&d));CHECK(ie_composite(&d,32,32)==0xff665544);
 b.erase=0;b.opacity=100;b.hardness=0;CHECK(ie_new(&d,64,64,0));CHECK(ie_brush_begin(&d));CHECK(ie_brush(&d,32,32,32,32,&b));CHECK((ie_composite(&d,32,32)>>24)==255);CHECK((ie_composite(&d,39,32)>>24)>0&&(ie_composite(&d,39,32)>>24)<128);
 CHECK(ie_new(&d,64,64,0));b.hardness=100;b.diffusion=100;CHECK(ie_brush_begin(&d));CHECK(ie_brush(&d,32,32,32,32,&b));{uint64_t h=digest(&d);CHECK(colored(&d)>10&&colored(&d)<100);for(i=0;i<12;i++)CHECK(ie_brush(&d,32,32,32,32,&b));CHECK(digest(&d)==h);for(i=0;i<64*64;i++)CHECK((d.current.layers[0].pixels[i]>>24)==0||(d.current.layers[0].pixels[i]>>24)==255);}
 CHECK(ie_new(&d,64,64,0));CHECK(ie_begin(&d));ie_shape(&d,IE_SHAPE_LINE,4,4,55,40,0xffffffff,50,17,0,1);for(i=0;i<64*64;i++)CHECK((d.current.layers[0].pixels[i]>>24)<=127);
 cleanup(&d,&m);
}
static void fill_pattern(IEDoc*d){int x,y;for(y=0;y<d->current.height;y++)for(x=0;x<d->current.width;x++)d->current.layers[d->current.active].pixels[(size_t)y*d->current.width+x]=IE_RGBA(x*11%256,y*7%256,(x*13+y*23)%256,((x+y)%5)?255:70);}
static void test_warp(void){IEDoc d;Memory m;int mode,i;setup(&d,&m,96,80);fill_pattern(&d);CHECK(ie_layer_duplicate(&d));ie_select(&d,20,20,75,65);
 for(mode=0;mode<3;mode++){uint64_t before=digest(&d),after;uint32_t old[96*80];size_t calls;memcpy(old,d.current.layers[d.current.active].pixels,sizeof(old));CHECK(ie_warp_begin(&d,48));calls=m.calls;CHECK(ie_warp_brush(&d,35,40,55,40,48,75,mode));CHECK(m.calls==calls);after=digest(&d);CHECK(after!=before);for(i=0;i<96*80;i++)if(i%96<20||i%96>75||i/96<20||i/96>65)CHECK(old[i]==d.current.layers[d.current.active].pixels[i]);CHECK(ie_cancel(&d));CHECK(digest(&d)==before);CHECK(!d.warp_scratch);CHECK(ie_warp_begin(&d,48));CHECK(ie_warp_brush(&d,35,40,55,40,48,75,mode));ie_commit(&d);CHECK(digest(&d)==after);CHECK(!d.warp_scratch);CHECK(ie_undo(&d));CHECK(digest(&d)==before);CHECK(ie_redo(&d));CHECK(digest(&d)==after);CHECK(ie_undo(&d));}
 CHECK(ie_warp_begin(&d,256));{size_t calls=m.calls;CHECK(ie_warp_brush(&d,-256,-256,350,350,256,100,IE_WARP_PUSH));CHECK(m.calls==calls);}CHECK(ie_cancel(&d));
 cleanup(&d,&m);
}
static void test_operation_faults(void){int mode,at,cycle;for(mode=0;mode<2;mode++)for(at=0;at<12;at++){IEDoc d;Memory m;uint64_t hash;unsigned undos,redos;setup(&d,&m,40,30);fill_pattern(&d);CHECK(ie_adjust(&d,1,0,0,100,100,100,0));CHECK(ie_undo(&d));hash=digest(&d);undos=d.undo_count;redos=d.redo_count;m.fail_at=(long)m.calls+at;m.persistent=1;{int ok=mode?ie_warp_begin(&d,256):ie_brush_begin(&d);if(!ok){CHECK(hash==digest(&d));if(at==0){CHECK(d.undo_count==undos);CHECK(d.redo_count==redos);}}else CHECK(ie_cancel(&d));}m.fail_at=-1;cleanup(&d,&m);}
 for(cycle=0;cycle<200;cycle++){IEDoc d;Memory m;IEBrush b={0xff123456,23,50,20,40,cycle%6,0};setup(&d,&m,48,32);CHECK(ie_brush_begin(&d));CHECK(ie_brush(&d,4,4,41,26,&b));CHECK(ie_cancel(&d));CHECK(ie_warp_begin(&d,23));CHECK(ie_warp_brush(&d,10,10,25,20,23,70,cycle%3));if(cycle&1)ie_commit(&d);else CHECK(ie_cancel(&d));cleanup(&d,&m);}
}
int main(void){legacy_main();test_opacity();test_warp();test_operation_faults();printf("{\"checks\":%u,\"repeated_cycles\":600,\"allocation_scenarios\":360,\"peak_bytes\":%zu,\"status\":\"passed\"}\n",checks,peak_bytes);return 0;}
