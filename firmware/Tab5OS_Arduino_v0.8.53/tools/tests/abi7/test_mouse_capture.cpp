#include "WriterMouse.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <deque>
struct lv_point_t{int x,y;};using lv_point_precise_t=lv_point_t;
struct lv_area_t{int x1,y1,x2,y2;};struct lv_obj_t{bool hidden=false;};struct lv_display_t{};
struct lv_indev_data_t{lv_point_t point;int state=0;};struct lv_indev_t{void(*read)(lv_indev_t*,lv_indev_data_t*)=nullptr;lv_indev_data_t data{};};
enum{LV_OBJ_FLAG_HIDDEN=1,LV_OBJ_FLAG_CLICKABLE=2,LV_OBJ_FLAG_SCROLLABLE=4,LV_INDEV_TYPE_POINTER=1,LV_INDEV_MODE_EVENT=1,LV_INDEV_STATE_RELEASED=0,LV_INDEV_STATE_PRESSED=1,LV_DIR_VER=1,LV_DIR_HOR=2,LV_ANIM_OFF=0};
static lv_obj_t systemLayer,topLayer,activeScreen,outerCursor,innerCursor;static lv_indev_t hostDevice;
bool lv_obj_has_flag(lv_obj_t* o,int flag){return flag==LV_OBJ_FLAG_HIDDEN?o->hidden:false;}
void lv_obj_add_flag(lv_obj_t* o,int flag){if(flag==LV_OBJ_FLAG_HIDDEN)o->hidden=true;}
void lv_obj_remove_flag(lv_obj_t* o,int flag){if(flag==LV_OBJ_FLAG_HIDDEN)o->hidden=false;}
void lv_obj_get_coords(lv_obj_t*,lv_area_t* a){*a={0,0,1279,719};}
unsigned lv_obj_get_child_count(lv_obj_t*){return 0;}lv_obj_t* lv_obj_get_child(lv_obj_t*,int){return nullptr;}
lv_obj_t* lv_obj_get_parent(lv_obj_t*){return nullptr;}lv_obj_t* lv_layer_sys(){return &systemLayer;}
lv_obj_t* lv_layer_top(){return &topLayer;}lv_obj_t* lv_screen_active(){return &activeScreen;}
lv_obj_t* lv_line_create(lv_obj_t* parent){return parent==&systemLayer?&outerCursor:&innerCursor;}
void lv_line_set_points(lv_obj_t*,const lv_point_precise_t*,int){}void lv_obj_set_style_line_width(lv_obj_t*,int,int){}
uint32_t lv_color_hex(uint32_t c){return c;}void lv_obj_set_style_line_color(lv_obj_t*,uint32_t,int){}
lv_indev_t* lv_indev_create(){return &hostDevice;}void lv_indev_set_type(lv_indev_t*,int){}void lv_indev_set_display(lv_indev_t*,lv_display_t*){}
void lv_indev_set_mode(lv_indev_t*,int){}void lv_indev_set_read_cb(lv_indev_t* d,void(*f)(lv_indev_t*,lv_indev_data_t*)){d->read=f;}
void lv_indev_read(lv_indev_t* d){if(d->read)d->read(d,&d->data);}void lv_indev_set_cursor(lv_indev_t*,lv_obj_t*){}
int lv_display_get_horizontal_resolution(void*){return 1280;}int lv_display_get_vertical_resolution(void*){return 720;}
int lv_obj_get_scroll_dir(lv_obj_t*){return 0;}int lv_obj_get_scroll_top(lv_obj_t*){return 0;}int lv_obj_get_scroll_bottom(lv_obj_t*){return 0;}
int lv_obj_get_scroll_left(lv_obj_t*){return 0;}int lv_obj_get_scroll_right(lv_obj_t*){return 0;}void lv_obj_scroll_by_bounded(lv_obj_t*,int,int,int){}
namespace Tab5UiStyle{void pointer(lv_indev_t*,int,int,uint8_t){}void pointerLeave(){}}
struct Focus{void pointerMode(){}}Tab5Focus;
namespace writing{
 bool connected=false;std::deque<MouseReport> reports;
 bool usbMouseConnected(){return connected;}
 bool usbMousePoll(MouseReport& report){if(reports.empty())return false;report=reports.front();reports.pop_front();return true;}
}
namespace Tab5Mouse{struct Config{uint16_t speed=100;uint8_t scroll=3;bool leftHanded=false,natural=false;};void capture(bool,int=0,int=0);}
#include "mouse_service.inc"
static int reportedX,reportedY,deltaX,deltaY;static unsigned reportedButtons,reports;
static bool raw(int x,int y,int dx,int dy,uint8_t buttons,int){reportedX=x;reportedY=y;deltaX=dx;deltaY=dy;reportedButtons=buttons;++reports;return true;}
static void feed(int16_t dx,int16_t dy,uint8_t buttons){writing::MouseReport r;r.x=dx;r.y=dy;r.buttons=buttons;writing::reports.push_back(r);Tab5Mouse::service();}
int main(){
 lv_display_t display;Tab5Mouse::begin(&display);Tab5Mouse::relativeEvents(raw);
 Tab5Mouse::capture(true,640,400);assert(!Tab5Mouse::captured()); // disconnected
 writing::connected=true;feed(1,1,0);int originalX=reportedX,originalY=reportedY;
 Tab5Mouse::capture(true,640,400);assert(Tab5Mouse::captured()&&outerCursor.hidden);
 for(int n=0;n<100;++n){feed(30000,-30000,1);assert(reportedX==640&&reportedY==400&&deltaX==30000&&deltaY==-30000&&reportedButtons==1);assert(hostDevice.data.state==LV_INDEV_STATE_RELEASED);}
 feed(-30000,30000,2);assert(reportedX==640&&reportedY==400&&reportedButtons==2);
 Tab5Mouse::capture(false);assert(!Tab5Mouse::captured()&&!outerCursor.hidden);feed(0,0,0);
 assert(reportedX==originalX&&reportedY==originalY&&!reportedButtons);
 Tab5Mouse::capture(true,640,400);feed(10,10,1);writing::connected=false;Tab5Mouse::service();
 assert(!Tab5Mouse::captured()&&outerCursor.hidden&&!reportedButtons&&hostDevice.data.state==LV_INDEV_STATE_RELEASED);
 writing::connected=true;feed(10,10,0);assert(!Tab5Mouse::captured()&&!outerCursor.hidden);
 assert(reports>=104);puts("Production mouse capture/service: unlimited signed USB deltas, hidden OS cursor, suppressed primary clicks, release, unplug and free reconnect passed.");
}
