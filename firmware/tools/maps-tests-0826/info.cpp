#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <lvgl.h>
#include "Tab5Focus.h"
using std::min;using std::max;
class String:public std::string {public:using std::string::string;String(const std::string& x):std::string(x){}String(size_t x):std::string(std::to_string(x)){};};
template<class T,class A,class B>T constrain(T x,A a,B b){return std::max(T(a),std::min(T(b),x));}
static unsigned checks=0;
static void check(bool ok,const char* s){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",s);exit(1);}}
static const char* T(const char* s){return s;}
static bool dark=true,mapDragging=false,mapInfoMode=false;
static lv_color_t C_TEXT(){return lv_color_hex(dark?0xeeeeee:0x111111);}
static lv_color_t C_BORDER(){return lv_color_hex(dark?0x556677:0x99aabb);}
static lv_color_t C_SURFACE(){return lv_color_hex(dark?0x1f252b:0xffffff);}
static lv_color_t C_RAISED(){return C_SURFACE();}
static lv_color_t C_BG(){return lv_color_hex(dark?0x101215:0xffffff);}
static lv_color_t C_ACTIVE(){return lv_color_hex(0x378abb);}
static lv_color_t C_SELECTED(){return C_ACTIVE();}
static const lv_font_t* uiFont(){return LV_FONT_DEFAULT;}
static const lv_font_t* richFontFor(int,int){return uiFont();}
static struct{int cornerRadius=12;} settings;
namespace Tab5UiStyle{void control(lv_obj_t*){}}
static constexpr int SCREEN_W=1280,SCREEN_H=720,MAP_VIEW_W=1172,MAP_VIEW_H=576;
static constexpr double MAP_PI=3.14159265358979323846;
static constexpr double MAP_MAX_LATITUDE=85.05112878;
static lv_obj_t *root=nullptr,*mapStylesOverlay=nullptr,*mapInfoButton=nullptr,*mapImage=nullptr;
static double mapLatitude=48.85,mapLongitude=2.35,mapPointLatitude=0,mapPointLongitude=0;
static uint8_t mapZoom=14;
static double mapHomeLatitude=0,mapHomeLongitude=0;static uint8_t mapHomeZoom=2;
static constexpr int MAP_MIN_ZOOM=0,MAP_MAX_ZOOM=22;
static lv_point_t mapDragStart{},mapDragLast{};
static unsigned renders=0,saves=0,pans=0;
static void syncMapZoomControl(){}static void panMapByPixels(int,int){++pans;}
static void markMapViewDirty(){}static void renderMapViewport(bool){++renders;}static void mapSavePlaceEvent(lv_event_t*){++saves;}
struct Tab5MapPlace{char name[64];double latitude,longitude;uint8_t zoom;};
static struct{size_t selectedStyle(){return 0;}String styleName(size_t){return "OpenStreetMap";}size_t placeCount(){return 0;}const Tab5MapPlace* place(size_t){return nullptr;}size_t offlinePackCountForSelectedStyle(){return 0;}}Tab5Map;
static uint8_t mapSourceZoom(){return mapZoom;}
static lv_obj_t* makeIconButton(lv_obj_t*,const char*,int,int);
static void mapPointInfo(int,int);
/* FUNCTIONS */
static lv_obj_t* makeIconButton(lv_obj_t* parent,const char* text,int w,int h){return makeButton(parent,text,w,h);}
static void createInfo(lv_obj_t* toolbar){
/* BUTTON */
}
static lv_obj_t* button(lv_obj_t* p,const char* text){if(lv_obj_check_type(p,&lv_button_class)&&lv_obj_get_child_count(p)){auto* l=lv_obj_get_child(p,0);if(lv_obj_check_type(l,&lv_label_class)&&!strcmp(lv_label_get_text(l),text))return p;}for(unsigned i=0;i<lv_obj_get_child_count(p);++i)if(auto* b=button(lv_obj_get_child(p,i),text))return b;return nullptr;}
int main(){
 lv_init();auto* d=lv_display_create(SCREEN_W,SCREEN_H);static uint8_t pixels[SCREEN_W*SCREEN_H*3];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(d,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,[](lv_display_t* display,const lv_area_t*,uint8_t*){lv_display_flush_ready(display);});
 root=lv_screen_active();Tab5Focus.begin(root,uiFont());auto* toolbar=lv_obj_create(root);lv_obj_set_size(toolbar,1172,56);mapImage=lv_obj_create(root);createInfo(toolbar);
 for(unsigned cycle=0;cycle<200;++cycle){dark=cycle%2==0;mapLatitude=48.85;mapLongitude=2.35;
  Tab5Focus.focusObject(mapInfoButton);lv_obj_send_event(mapInfoButton,LV_EVENT_CLICKED,nullptr);
  check(mapStylesOverlay&&lv_obj_is_valid(mapStylesOverlay),"I button opens point information immediately");check(fabs(mapPointLatitude-48.85)<0.00001&&fabs(mapPointLongitude-2.35)<0.00001,"I uses map centre");
  lv_obj_update_layout(root);auto* panel=lv_obj_get_child(mapStylesOverlay,0);lv_area_t a;lv_obj_get_coords(panel,&a);check(a.x1>=0&&a.y1>=0&&a.x2<SCREEN_W&&a.y2<SCREEN_H,"information panel fits screen");
  const char* caption=cycle%2==0?"Fermer":"Centrer ici";auto* b=button(panel,caption);check(b!=nullptr,"information action visible");Tab5Focus.focusObject(b);writing::KeyEvent enter{};enter.key=writing::Key::Enter;check(Tab5Focus.key(enter,mapStylesOverlay),"keyboard executes focused action");check(!mapStylesOverlay&&!Tab5Focus.focused(),"closing information clears OS focus");
 }
 check(renders==100,"centre action actually renders view");
 mapPointInfo(0,0);check(mapPointLatitude>mapLatitude&&mapPointLongitude<mapLongitude,"selected point has distinct coordinates");closeMapStylesEvent(nullptr);
 // Real LVGL pointer samples, including its long-press timer; only panning's
 // raster/network boundary is mocked. A long press must not pan on release.
 lv_obj_set_pos(mapImage,8,72);lv_obj_set_size(mapImage,MAP_VIEW_W,MAP_VIEW_H);lv_obj_add_flag(mapImage,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(mapImage,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(mapImage,mapTouchEvent,LV_EVENT_ALL,nullptr);lv_obj_update_layout(root);
 auto* pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_display(pointer,d);lv_indev_set_long_press_time(pointer,400);
 static lv_point_t point{200,200};static bool held=false;
 lv_indev_set_read_cb(pointer,[](lv_indev_t*,lv_indev_data_t* data){data->point=point;data->state=held?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;});
 held=true;lv_indev_read(pointer);lv_tick_inc(500);lv_indev_read(pointer);check(mapStylesOverlay!=nullptr,"long press opens selected point");
 held=false;lv_indev_read(pointer);check(!mapDragging&&pans==0,"long press release never pans");closeMapStylesEvent(nullptr);
 point={300,300};held=true;lv_indev_read(pointer);point={330,300};lv_indev_read(pointer);held=false;lv_indev_read(pointer);check(pans==1&&!mapStylesOverlay,"drag remains panning");
 uint32_t key='I';lv_obj_send_event(mapImage,LV_EVENT_KEY,&key);check(mapStylesOverlay!=nullptr,"physical I opens information");closeMapStylesEvent(nullptr);
 lv_indev_delete(pointer);
 printf("PASS %u map info checks; actual LVGL and production controls; 200 open/action/close cycles\n",checks);
}
