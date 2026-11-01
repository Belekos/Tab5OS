#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>
#include <lvgl.h>
#include "Tab5MapViewPolicy.h"
#include "Tab5TileRaster.h"
using std::min;using std::max;
template<class T>T constrain(T v,T lo,T hi){return std::max(lo,std::min(v,hi));}
static unsigned checks=0,loads=0,dirty=0;
static void check(bool value,const char* why){++checks;if(!value){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
struct Engine {
 uint8_t maximum=19;int8_t shift=0;
 size_t selectedStyle(){return 0;}
 uint8_t styleMinZoom(size_t){return 0;}
 uint8_t styleMaxZoom(size_t){return maximum;}
 uint8_t styleViewMaxZoom(size_t){return Tab5MapViewPolicy::displayMax(maximum,shift);}
 uint8_t styleSourceZoom(size_t,uint8_t z){return Tab5MapViewPolicy::sourceZoom(z,maximum,shift);}
}Tab5Map;
static constexpr uint8_t MAP_MAX_ZOOM=22;
static constexpr double MAP_MAX_LATITUDE=85.05112878,MAP_PI=3.14159265358979323846;
static constexpr int MAP_VIEW_W=1172,MAP_VIEW_H=576;
static double mapLatitude=47.7,mapLongitude=-3.4;
static uint8_t mapZoom=19;
static bool mapZoomDragging=false;
static lv_obj_t *mapZoomSlider=nullptr,*mapZoomValueLabel=nullptr;
static void markMapViewDirty(){++dirty;}
static void renderMapViewport(bool);
/* FUNCTIONS */
static void renderMapViewport(bool){++loads;clampMapView();syncMapZoomControl();}
namespace Tab5TileValidation {
Result decode(const uint8_t*,size_t,uint16_t* pixels,bool(*)()){
 for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x)pixels[y*256+x]=uint16_t((y<<8)|x);
 return Result::Valid;
}
}
int main(){
 lv_init();auto* display=lv_display_create(1280,720);check(display!=nullptr,"LVGL display");
 mapZoomSlider=lv_slider_create(lv_screen_active());mapZoomValueLabel=lv_label_create(lv_screen_active());
 lv_obj_add_event_cb(mapZoomSlider,mapZoomSliderEvent,LV_EVENT_ALL,nullptr);syncMapZoomControl();
 check(lv_slider_get_max_value(mapZoomSlider)==22,"OSM slider reaches22");
 lv_obj_send_event(mapZoomSlider,LV_EVENT_PRESSED,nullptr);lv_slider_set_value(mapZoomSlider,22,LV_ANIM_OFF);
 lv_obj_send_event(mapZoomSlider,LV_EVENT_VALUE_CHANGED,nullptr);
 check(mapZoom==19&&loads==0,"drag previews without tile loads");
 check(std::string(lv_label_get_text(mapZoomValueLabel))=="z22","preview requested zoom");
 lv_obj_send_event(mapZoomSlider,LV_EVENT_RELEASED,nullptr);
 check(mapZoom==22&&mapSourceZoom()==19&&mapSourceTileScreenSize()==2048&&loads==1,"release loads native19 at display22");
 lv_slider_set_value(mapZoomSlider,21,LV_ANIM_OFF);lv_obj_send_event(mapZoomSlider,LV_EVENT_VALUE_CHANGED,nullptr);
 check(mapZoom==21&&loads==2,"keyboard value commits");
 lv_obj_send_event(mapZoomSlider,LV_EVENT_PRESSED,nullptr);lv_slider_set_value(mapZoomSlider,18,LV_ANIM_OFF);
 lv_obj_send_event(mapZoomSlider,LV_EVENT_CANCEL,nullptr);
 check(mapZoom==21&&loads==2&&lv_slider_get_value(mapZoomSlider)==21,"cancel restores committed value");
 for(auto style:std::vector<std::pair<uint8_t,int8_t>>{{19,0},{20,-1},{17,0},{22,0},{5,-2}}){
  Tab5Map.maximum=style.first;Tab5Map.shift=style.second;
  for(unsigned view=0;view<=22;++view){
   mapZoom=view;clampMapView();syncMapZoomControl();
   auto z=mapSourceZoom();check(z<=Tab5MapViewPolicy::nativeMax(style.first,style.second),"no missing source zoom requested");
   check(mapZoom-z<=5,"bounded overzoom");
   int64_t x0,x1,y0,y1;double left,top;mapVisibleBounds(x0,x1,y0,y1,left,top);
   check(x1>=x0&&y1>=y0&&(x1-x0+1)*(y1-y0+1)<=24,"bounded viewport jobs");
   for(double lon:{-179.999,0.0,179.999})for(double lat:{-85.0,0.0,47.7,85.0}){
    double outLat=0,outLon=0;mapCoordinatesFromWorld(mapWorldX(lon,mapZoom),mapWorldY(lat,mapZoom),mapZoom,outLat,outLon);
    check(fabs(mapWorldY(lat,mapZoom)-mapWorldY(outLat,mapZoom))<=1.00001&&fabs(lon-outLon)<0.00001,"coordinate roundtrip within polar edge pixel");
   }
  }
 }
 // Production raster draws only the clipped canvas. A x32 parent never allocates x32 pixels.
 std::vector<uint16_t> pixels(320*160);uint8_t data=0;
 for(int scale:{1,2,4,8,16,32}){
  const int size=256*scale,x=-size/3,y=-size/4;
  check(Tab5TileRaster::draw(pixels.data(),320,160,&data,1,x,y,size),"production raster overzoom");
  const unsigned dx=20,dy=25,sx=(dx-x)*256/size,sy=(dy-y)*256/size;
  check(pixels[dy*320+dx]==uint16_t((sx<<8)|sy),"parent pixel matches clipped view");
 }
 lv_deinit();printf("PASS %u production zoom/LVGL/geometry/raster checks\n",checks);return 0;
}
