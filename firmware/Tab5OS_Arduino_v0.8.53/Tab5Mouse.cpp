#include "Tab5Mouse.h"
#include "WriterUsb.h"
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include <algorithm>
namespace Tab5Mouse {
namespace {
lv_indev_t* input=nullptr;lv_obj_t* cursor=nullptr;lv_obj_t* cursorInside=nullptr;uint32_t pointerColor=0xffaa20,outlineColor=0x101214;int x=320,y=240;uint8_t buttons=0;bool showing=false;Config config;int fractionX=0,fractionY=0;void(*pageScrollHook)(int)=nullptr;
const lv_point_precise_t points[]={{0,0},{0,19},{5,14},{9,21},{12,19},{8,12},{15,12},{0,0}};
void (*contextMenuHook)(lv_obj_t*,int,int)=nullptr;
bool (*pointerEventsHook)(int,int,uint8_t,int)=nullptr;
bool (*relativeEventsHook)(int,int,int,int,uint8_t,int)=nullptr;
bool captureActive=false,captureSuppressButtons=false;int captureX=0,captureY=0,returnX=0,returnY=0;
lv_obj_t* hover(lv_obj_t* object,const lv_point_t& point,unsigned depth=0){
 if(depth>24||lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))return nullptr;
 lv_area_t area;lv_obj_get_coords(object,&area);if(point.x<area.x1||point.x>area.x2||point.y<area.y1||point.y>area.y2)return nullptr;
 for(int i=int(lv_obj_get_child_count(object))-1;i>=0;--i){auto* found=hover(lv_obj_get_child(object,i),point,depth+1);if(found)return found;}
 return object;
}
}
void configure(const Config& value){config=value;config.speed=std::clamp<uint16_t>(config.speed,25,300);config.scroll=std::clamp<uint8_t>(config.scroll,1,12);fractionX=fractionY=0;}
void palette(uint32_t pointer,uint32_t outline){pointerColor=pointer;outlineColor=outline;if(cursor)lv_obj_set_style_line_color(cursor,lv_color_hex(outline),0);if(cursorInside)lv_obj_set_style_line_color(cursorInside,lv_color_hex(pointer),0);}
void pageScroll(void (*callback)(int)){pageScrollHook=callback;}
void contextMenu(void (*callback)(lv_obj_t*,int,int)){contextMenuHook=callback;}
void pointerEvents(bool (*callback)(int,int,uint8_t,int)){pointerEventsHook=callback;}
void relativeEvents(bool (*callback)(int,int,int,int,uint8_t,int)){relativeEventsHook=callback;}
bool captured(){return captureActive;}
void capture(bool enabled,int anchorX,int anchorY){
 enabled=enabled&&writing::usbMouseConnected();if(enabled==captureActive)return;
 if(enabled){returnX=x;returnY=y;captureX=anchorX;captureY=anchorY;}
 if(!enabled&&buttons)captureSuppressButtons=true;
 captureActive=enabled;x=enabled?captureX:returnX;y=enabled?captureY:returnY;fractionX=fractionY=0;
 if(input)lv_indev_read(input);
 if(cursor){if(enabled||!writing::usbMouseConnected())lv_obj_add_flag(cursor,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(cursor,LV_OBJ_FLAG_HIDDEN);}
}
void begin(lv_display_t* display){
 input=lv_indev_create();if(!input)return;lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_display(input,display);lv_indev_set_mode(input,LV_INDEV_MODE_EVENT);
 lv_indev_set_read_cb(input,[](lv_indev_t*,lv_indev_data_t* d){d->point={x,y};d->state=!captureActive&&!captureSuppressButtons&&(buttons&(config.leftHanded?2:1))?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;});
}
bool isInput(const lv_indev_t* device){return device&&device==input;}
bool service(){
 if(!input)return false;bool connected=writing::usbMouseConnected();
 const bool notifyDisconnect=!connected&&(showing||buttons);
 if(connected&&!cursor){cursor=lv_line_create(lv_layer_sys());lv_line_set_points(cursor,points,8);lv_obj_set_style_line_width(cursor,5,0);lv_obj_set_style_line_color(cursor,lv_color_hex(outlineColor),0);cursorInside=lv_line_create(cursor);lv_line_set_points(cursorInside,points,8);lv_obj_set_style_line_width(cursorInside,2,0);lv_obj_set_style_line_color(cursorInside,lv_color_hex(pointerColor),0);lv_obj_remove_flag(cursorInside,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(cursor,LV_OBJ_FLAG_CLICKABLE);lv_indev_set_cursor(input,cursor);}
 if(cursor&&connected!=showing){if(connected&&!captureActive)lv_obj_remove_flag(cursor,LV_OBJ_FLAG_HIDDEN);else{if(!connected)buttons=0;lv_indev_read(input);lv_obj_add_flag(cursor,LV_OBJ_FLAG_HIDDEN);}showing=connected;}
 if(!connected){
  capture(false);
  // Old queue entries can outlive USB disconnection, including a held button
  // when the final release could not be queued. Never replay them into LVGL
  // or open a context menu after the device has gone away.
  if(buttons){buttons=0;lv_indev_read(input);}fractionX=fractionY=0;captureSuppressButtons=false;
  if(notifyDisconnect){Tab5UiStyle::pointerLeave();if(relativeEventsHook)relativeEventsHook(x,y,0,0,0,0);if(pointerEventsHook)pointerEventsHook(x,y,0,0);}
  writing::MouseReport stale;
  for(unsigned count=0;count<32&&writing::usbMousePoll(stale);++count){}
  return false;
 }
 bool activity=false,hoverPending=false;writing::MouseReport report;
 auto flushHover=[&](){if(!hoverPending)return;Tab5Focus.pointerMode();lv_indev_read(input);
  Tab5UiStyle::pointer(input,x,y,0);
  if(pointerEventsHook)pointerEventsHook(x,y,0,0);hoverPending=false;};
 for(unsigned count=0;count<32&&writing::usbMousePoll(report);++count){
  const int width=lv_display_get_horizontal_resolution(nullptr),height=lv_display_get_vertical_resolution(nullptr);
  const uint8_t previousButtons=buttons;
  if(!report.buttons)captureSuppressButtons=false;
  const bool changed=report.x||report.y||report.wheel||report.horizontal||report.buttons!=buttons;
  if(!changed){
   // Event-mode LVGL still needs held-button samples for long press/repeat.
   // A stationary released HID heartbeat must not steal keyboard navigation.
   if(buttons)lv_indev_read(input);
   continue;
  }
  const bool hoverOnly=!previousButtons&&!report.buttons&&!report.wheel&&!report.horizontal;
  // Keep every fractional delta and edge clamp, but dispatch only the latest
  // hover position in this bounded batch. Button/drag/wheel events stay ordered.
  if(!hoverOnly)flushHover();
  x=captureActive?captureX:std::max(0,std::min(width-1,x+(fractionX+int(report.x)*config.speed)/100));y=captureActive?captureY:std::max(0,std::min(height-1,y+(fractionY+int(report.y)*config.speed)/100));fractionX=captureActive?0:(fractionX+int(report.x)*config.speed)%100;fractionY=captureActive?0:(fractionY+int(report.y)*config.speed)%100;buttons=report.buttons;
  const uint8_t logicalButtons=(buttons&4)|((buttons&(config.leftHanded?2:1))?1:0)|((buttons&(config.leftHanded?1:2))?2:0);
  const bool relativeConsumed=relativeEventsHook&&relativeEventsHook(x,y,report.x,report.y,logicalButtons,int(report.wheel)*(config.natural?-1:1));
  activity=true;if(hoverOnly){hoverPending=true;continue;}
  Tab5Focus.pointerMode();lv_indev_read(input);
  Tab5UiStyle::pointer(input,x,y,buttons);
  if(relativeConsumed)continue;
  if(pointerEventsHook&&pointerEventsHook(x,y,logicalButtons,int(report.wheel)*(config.natural?-1:1)))continue;
  const uint8_t secondary=config.leftHanded?1:2;
  if(contextMenuHook&&(buttons&secondary)&&!(previousButtons&secondary)&&!(buttons&(config.leftHanded?2:1))){
    lv_point_t point{x,y};auto* target=hover(lv_layer_top(),point);
    if(!target||target==lv_layer_top())target=hover(lv_screen_active(),point);
    contextMenuHook(target,x,y);
  }
  if(report.wheel||report.horizontal){lv_point_t p{x,y};auto* target=hover(lv_layer_top(),p);if(!target||target==lv_layer_top())target=hover(lv_screen_active(),p);
   bool scrolled=false;const int direction=config.natural?-1:1;
   for(;target;target=lv_obj_get_parent(target))if(lv_obj_has_flag(target,LV_OBJ_FLAG_SCROLLABLE)){
     const bool vertical=report.wheel&&(lv_obj_get_scroll_dir(target)&LV_DIR_VER)&&(lv_obj_get_scroll_top(target)>0||lv_obj_get_scroll_bottom(target)>0);
     const bool horizontal=report.horizontal&&(lv_obj_get_scroll_dir(target)&LV_DIR_HOR)&&(lv_obj_get_scroll_left(target)>0||lv_obj_get_scroll_right(target)>0);
     if(vertical||horizontal){lv_obj_scroll_by_bounded(target,horizontal?-int(report.horizontal)*config.scroll*16*direction:0,vertical?int(report.wheel)*config.scroll*16*direction:0,LV_ANIM_OFF);scrolled=true;break;}}
   if(!scrolled&&report.wheel&&pageScrollHook)pageScrollHook(report.wheel*direction);
   Tab5UiStyle::pointer(input,x,y,buttons);
  }
 }
 flushHover();
 return activity;
}
}
