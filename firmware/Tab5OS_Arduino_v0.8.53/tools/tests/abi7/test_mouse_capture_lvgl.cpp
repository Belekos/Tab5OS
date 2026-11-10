#include <lvgl.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include "Tab5Mouse.h"
#include "WriterMouse.h"

namespace writing {
bool connected=true;
std::deque<MouseReport> reports;
bool usbMouseConnected(){return connected;}
bool usbMousePoll(MouseReport& report){
 if(reports.empty())return false;
 report=reports.front();reports.pop_front();return true;
}
}
namespace Tab5UiStyle {
void pointer(lv_indev_t*,int,int,uint8_t){}
void pointerLeave(){}
}
struct FocusStub{void pointerMode(){}} Tab5Focus;
#ifdef TEST_SYNCHRONOUS_CAPTURE_READ
#include "unsafe_mouse_service.inc"
#else
#include "mouse_service.inc"
#endif

static int clicks=0,depth=0;
static lv_indev_t* expected=nullptr;
static lv_indev_data_t touchSample;
static void require(bool condition,const char* message){
 if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::fflush(stderr);std::exit(2);}
}
static void captureClick(lv_event_t*){
 require(++depth==1,"recursive LV_EVENT_CLICKED from capture transition");
 ++clicks;
 auto* active=lv_indev_active();
 require(active!=nullptr,"click must originate from an actual LVGL input read");
 require(expected?active==expected:Tab5Mouse::isInput(active),"incorrect active input entering callback");
 Tab5Mouse::capture(!Tab5Mouse::captured(),640,400);
 require(lv_indev_active()==active,"capture cleared or replaced active input inside LV_EVENT_CLICKED");
 require(!Tab5Mouse::service(),"service must defer while an input callback is active");
 require(lv_indev_active()==active,"nested service changed the active input context");
 --depth;
}
static void touchRead(lv_indev_t*,lv_indev_data_t* sample){*sample=touchSample;}
static lv_indev_t* makeTouch(lv_display_t* display){
 auto* device=lv_indev_create();lv_indev_set_type(device,LV_INDEV_TYPE_POINTER);
 lv_indev_set_display(device,display);lv_indev_set_mode(device,LV_INDEV_MODE_EVENT);
 lv_indev_set_read_cb(device,touchRead);return device;
}
static void touch(lv_indev_t* input,int px,int py,lv_indev_state_t state){
 touchSample.point={px,py};touchSample.state=state;
 lv_tick_inc(20);lv_indev_read(input);
}
static void mouse(int dx,int dy,uint8_t buttons){
 writing::MouseReport report;report.x=static_cast<int16_t>(dx);report.y=static_cast<int16_t>(dy);report.buttons=buttons;
 writing::reports.push_back(report);lv_tick_inc(20);Tab5Mouse::service();
}
int main(int argc,char** argv){
 require(argc==2,"scenario argument required");
 lv_init();auto* display=lv_display_create(1280,720);
 auto* button=lv_button_create(lv_screen_active());
 lv_obj_set_pos(button,800,10);lv_obj_set_size(button,196,64);
 lv_obj_remove_flag(button,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_add_event_cb(button,captureClick,LV_EVENT_CLICKED,nullptr);
 lv_obj_update_layout(lv_screen_active());
 Tab5Mouse::begin(display);Tab5Mouse::service();
 if(std::strcmp(argv[1],"touch")==0){
  expected=makeTouch(display);
  touch(expected,850,40,LV_INDEV_STATE_PRESSED);
  touch(expected,850,40,LV_INDEV_STATE_RELEASED);
  require(clicks==1&&Tab5Mouse::captured(),"touch release must capture once");
  Tab5Mouse::service();
  require(clicks==1,"deferred capture caused an extra click");
  touch(expected,850,40,LV_INDEV_STATE_PRESSED);
  touch(expected,850,40,LV_INDEV_STATE_RELEASED);
  require(clicks==2&&!Tab5Mouse::captured(),"second touch release must uncapture once");
  Tab5Mouse::service();
  require(clicks==2,"deferred release caused an extra click");
 }else if(std::strcmp(argv[1],"mouse")==0){
  mouse(530,-200,0);mouse(0,0,1);mouse(0,0,0);
  require(clicks==1&&Tab5Mouse::captured(),"mouse release must capture once");
  Tab5Mouse::service();
  require(clicks==1,"deferred capture caused an extra mouse click");
  Tab5Mouse::capture(false);Tab5Mouse::service();
  mouse(0,0,1);mouse(0,0,0);
  require(clicks==2&&Tab5Mouse::captured(),"restored mouse must click capture button again");
 }else if(std::strcmp(argv[1],"held-release")==0){
  mouse(530,-200,0);Tab5Mouse::capture(true,640,400);Tab5Mouse::service();
  mouse(0,0,1);require(clicks==0,"captured held mouse reached OS button");
  expected=makeTouch(display);
  touch(expected,850,40,LV_INDEV_STATE_PRESSED);touch(expected,850,40,LV_INDEV_STATE_RELEASED);
  require(clicks==1&&!Tab5Mouse::captured(),"touch failed to release mouse capture");
  Tab5Mouse::service();mouse(0,0,1);mouse(0,0,0);
  require(clicks==1,"held mouse synthesized an OS click on capture release");
  expected=nullptr;mouse(0,0,1);mouse(0,0,0);
  require(clicks==2&&Tab5Mouse::captured(),"fresh mouse click remained suppressed");
 }else if(std::strcmp(argv[1],"disconnect")==0){
  mouse(530,-200,0);mouse(0,0,1);
  writing::connected=false;writing::MouseReport stale;stale.buttons=0;writing::reports.push_back(stale);
  Tab5Mouse::service();require(clicks==0,"disconnect synthesized a click for held mouse");
  require(writing::reports.empty(),"disconnected USB reports were not drained");
  writing::connected=true;Tab5Mouse::service();mouse(0,0,1);mouse(0,0,0);
  require(clicks==1&&Tab5Mouse::captured(),"reconnected mouse failed to capture");
  mouse(0,0,1);writing::connected=false;Tab5Mouse::service();
  require(clicks==1&&!Tab5Mouse::captured(),"disconnect failed to cancel captured held mouse");
  writing::connected=true;Tab5Mouse::service();mouse(0,0,1);mouse(0,0,0);
  require(clicks==2&&Tab5Mouse::captured(),"mouse remained suppressed after reconnect");
 }else require(false,"unknown scenario");
 std::printf("PASS real LVGL %s: %d clicks, no nested input read\n",argv[1],clicks);
 return 0;
}
