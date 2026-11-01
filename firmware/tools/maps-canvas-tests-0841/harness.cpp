#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include "Tab5MapWheel.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
struct lv_obj_t{};static lv_obj_t imageObject,statusObject;
static unsigned invalidations=0,fills=0,rectangles=0,labels=0,viewports=0,generations=0;
static void lv_obj_invalidate(lv_obj_t*){++invalidations;}
static bool lv_obj_is_valid(lv_obj_t*p){return p!=nullptr;}
static void lv_obj_delete(lv_obj_t*){}
static void lv_image_set_src(lv_obj_t*,void*){}
static void setLabelIfChanged(lv_obj_t*,const char*){++labels;}
static const char*T(const char*s){return s;}
static void updateMapLabels(){++labels;}
static void clampMapView(){}static void syncMapZoomControl(){}
static int MAP_VIEW_W=512,MAP_VIEW_H=512;
struct M5Canvas{
 int w,h;std::vector<uint16_t> pixels;
 M5Canvas():w(MAP_VIEW_W),h(MAP_VIEW_H),pixels(size_t(w)*h){}
 void fillSprite(uint32_t color){++fills;std::fill(pixels.begin(),pixels.end(),uint16_t(color));}
 void drawRect(int x,int y,int width,int height,uint32_t color){++rectangles;for(int py=std::max(0,y);py<std::min(h,y+height);++py)for(int px=std::max(0,x);px<std::min(w,x+width);++px)if(py==y||py==y+height-1||px==x||px==x+width-1)pixels[size_t(py)*w+px]=uint16_t(color);}
 void deleteSprite(){pixels.clear();}
};
static M5Canvas*mapCanvas=nullptr;static bool allocationOk=true;
static bool ensureMapCanvas(){if(!allocationOk)return false;if(!mapCanvas)mapCanvas=new M5Canvas;return true;}
static lv_obj_t*mapImage=&imageObject,*mapStatusLabel=&statusObject;
static lv_obj_t *mapPlacesDeleteOverlay=nullptr,*mapPlacesOverlay=nullptr,*mapStylesOverlay=nullptr,*mapSearchOverlay=nullptr,*mapDownloadOverlay=nullptr,*mapPlacesList=nullptr,*mapSelectedPlaceButton=nullptr;
static struct {unsigned magic=0;}mapImageDsc;
static double mapLatitude=50,mapLongitude=-3;static uint8_t mapZoom=8;
static uint32_t mapGeneration=0;
static Tab5MapWheel::View mapRenderedView;static size_t mapRenderedStyle=0;
static int mapRenderedWidth=0,mapRenderedHeight=0;static uint32_t mapRenderedGeneration=0;
static uint16_t mapTilesNeeded=0,mapTilesReady=0,mapTilesPending=0,mapTilesFailed=0;
static int mapLastHttpError=0,mapLastTlsError=0;static bool mapRasterRefresh=false;
struct MapRasterJob{uint32_t x,y;uint16_t distance;};
static MapRasterJob jobs[32],*mapRasterJobs=jobs;static unsigned mapRasterIndex=0,mapRasterCount=0;
static unsigned mapSourceZoom(){return mapZoom;}
static int mapSourceTileScreenSize(){return 256;}
static void mapVisibleBounds(int64_t&fx,int64_t&lx,int64_t&fy,int64_t&ly,double&left,double&top){fx=fy=0;lx=ly=1;left=top=0;}
static struct{bool available=true;size_t style=0;bool ready(){return available;}size_t selectedStyle(){return style;}void beginGeneration(uint32_t){++generations;}void clearMemoryCache(){}}Tab5Map;
namespace Tab5MapOverlay{static void viewport(double,double,unsigned,int,int){++viewports;}}
static struct{template<class...Args>void printf(const char*,Args...){}}Tab5DebugSerial;
/* FUNCTIONS */
static void render(bool refresh=false,bool preserve=false){
#if FIXED
 renderMapViewport(refresh,preserve);
#else
 (void)preserve;renderMapViewport(refresh);
#endif
}
static void downloaded(){for(size_t n=0;n<mapCanvas->pixels.size()/2;++n)mapCanvas->pixels[n]=uint16_t(0x7200+(n%256));}
static void reset(){freeMapCanvas();mapGeneration=0;mapRenderedGeneration=0;mapLatitude=50;mapLongitude=-3;mapZoom=8;MAP_VIEW_W=MAP_VIEW_H=512;Tab5Map.style=0;Tab5Map.available=true;allocationOk=true;mapRasterJobs=jobs;invalidations=fills=rectangles=labels=viewports=generations=0;}
int main(){
 reset();render();CHECK(fills==1&&rectangles==4&&invalidations==1&&mapTilesNeeded==4&&mapTilesPending==4&&mapRasterCount==4);
 CHECK(mapRasterIndex==0&&!mapRasterRefresh&&mapGeneration==1&&generations==1);
 downloaded();const auto original=mapCanvas->pixels;
 // Identical partial frame survives repeated Wi-Fi/audio resumes. Jobs still
 // regenerate and retain progressive priority, without changing any pixels.
 for(unsigned n=0;n<100;++n){unsigned oldGen=mapGeneration;render(false,true);CHECK(mapGeneration==oldGen+1&&mapTilesPending==4&&mapRasterCount==4&&mapRasterIndex==0);CHECK(std::is_sorted(jobs,jobs+4,[](auto a,auto b){return a.distance<b.distance;}));}
 const unsigned resumeFills=fills,resumeInvalidations=invalidations;
#if FIXED
 CHECK(fills==1&&rectangles==4&&invalidations==1&&mapCanvas->pixels==original);CHECK(mapRenderedGeneration==mapGeneration);
#else
 CHECK(fills==101&&rectangles==404&&invalidations==101&&mapCanvas->pixels!=original);
#endif
 // All real changes and explicit refreshes discard the old view.
 auto mustClear=[&](bool refresh=false,bool preserve=true){unsigned f=fills,i=invalidations;downloaded();render(refresh,preserve);CHECK(fills==f+1&&invalidations==i+1);CHECK(mapCanvas->pixels!=original);};
 mapLatitude+=1;mustClear();mapLongitude+=1;mustClear();++mapZoom;mustClear();++Tab5Map.style;mustClear();++MAP_VIEW_W;mustClear();++MAP_VIEW_H;mustClear();++mapGeneration;mustClear();mustClear(true,true);mustClear(false,false);
 // No stale eligibility after engine unavailable / jobs unavailable / failed
 // allocation. The following successful identical resume must clear again.
 for(unsigned which=0;which<3;++which){reset();render();downloaded();unsigned f=fills;
  if(which==0)Tab5Map.available=false;if(which==1)mapRasterJobs=nullptr;if(which==2)allocationOk=false;
  render(false,true);CHECK(fills==f);
#if FIXED
  CHECK(mapRenderedGeneration==0);
#endif
  Tab5Map.available=true;mapRasterJobs=jobs;allocationOk=true;render(false,true);CHECK(fills==f+1);
 }
 // First/after-free/reallocated frames must not inherit a stamp, even if the
 // allocator happens to return the same address. Generation wrap skips zero.
 reset();render(false,true);CHECK(fills==1);downloaded();freeMapCanvas();
#if FIXED
 CHECK(mapRenderedGeneration==0);
#endif
 render(false,true);CHECK(fills==2&&mapCanvas);mapGeneration=UINT32_MAX;render(false,true);CHECK(mapGeneration==1);
 // Returning from a generation owned by print/export is also a real invalidation.
 downloaded();++mapGeneration;unsigned f=fills;render(false,true);CHECK(fills==f+1);
 freeMapCanvas();CHECK(mapCanvas==nullptr&&mapRasterCount==0&&mapRasterIndex==0);
 std::cout<<"checks="<<checks<<" PASS resumeFills="<<resumeFills<<" resumeInvalidations="<<resumeInvalidations<<" simulatedPartialPixels="<<original.size()/2<<"\n";
}
