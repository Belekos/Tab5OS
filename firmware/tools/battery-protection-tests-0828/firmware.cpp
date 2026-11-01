#include <lvgl.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Tab5BatteryGuard.h"
using String=std::string;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
namespace Tab5BatteryGuard {static Result current;const Result& status(){return current;}}
namespace Tab5Print{static bool active;bool busy(){return active;}}
namespace Tab5Vault{static bool active;bool busy(){return active;}}
namespace Tab5Python{static bool active;bool running(){return active;}}
namespace Tab5MapPrint{static bool active;bool busy(){return active;}}
static bool executableActive=false;static bool tab5ExecutableBusy(){return executableActive;}
namespace Tab5Firmware {
 static bool active=false,complete=false,reviewed=false;
 static unsigned installs=0,restarts=0,inspections=0,cancels=0;
 void clearReview(){reviewed=false;complete=false;}
 bool inspect(const char*){++inspections;return true;}
 bool install(const char*){++installs;return true;}
 void cancel(){++cancels;}
 void restart(){++restarts;}
 bool busy(){return active;}bool installed(){return complete;}bool ready(){return reviewed;}
 const char* status(){return "status";}unsigned progress(){return 40;}
 String description(){return "description";}
}
enum class Tab5Tool {Firmware};
struct FirmwareUI {
 Tab5Tool tool=Tab5Tool::Firmware;
 lv_obj_t *root=nullptr,*path=nullptr,*output=nullptr,*firmwareInstall=nullptr,*firmwareRestart=nullptr,*firmwareProgress=nullptr,*firmwareCancel=nullptr,*firmwareVerify=nullptr;
 bool firmwareUpdates=false;
 String lastOutput,lastNotice;
 static uint32_t color(unsigned n){return n==4?0x00ffff:0x333333;}
 void note(const char* text){lastNotice=text;}
 /* enabled */
 lv_obj_t* label(const char* text,int x,int y,int width){auto*o=lv_label_create(root);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);lv_label_set_text(o,text);return o;}
 lv_obj_t* field(const char* text,int x,int y,int width){auto*o=lv_textarea_create(root);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,48);lv_textarea_set_text(o,text);return o;}
 lv_obj_t* button(const char* text,int x,int y,int width,int){auto*o=lv_button_create(root);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,50);auto*l=lv_label_create(o);lv_label_set_text(l,text);return o;}
 void build(const char* selected){root=lv_obj_create(lv_screen_active());lv_obj_set_size(root,1180,650);auto which=Tab5Tool::Firmware;/* build */}
 void act(int action){/* act */}
 void service(){auto*a=this;/* service */}
 ~FirmwareUI(){if(root)lv_obj_delete(root);}
};
static bool enabled(lv_obj_t*o){return !lv_obj_has_state(o,LV_STATE_DISABLED);}
static bool visible(lv_obj_t*o){return !lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN);}
static void reset(){Tab5Print::active=Tab5Vault::active=Tab5Python::active=Tab5MapPrint::active=executableActive=false;Tab5Firmware::active=Tab5Firmware::complete=Tab5Firmware::reviewed=false;Tab5BatteryGuard::current.countdown=false;}
int main(){
 lv_init();auto* display=lv_display_create(1280,720);static uint8_t buffer[1280*12*4];
 lv_display_set_buffers(display,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});
 for(const char* path: {static_cast<const char*>(nullptr),"","/firmware.bin"})for(bool countdown:{false,true}) {
  reset();Tab5BatteryGuard::current.countdown=countdown;FirmwareUI ui;ui.build(path);
  CHECK(enabled(ui.firmwareVerify)==(path&&*path));CHECK(enabled(ui.firmwareInstall)==(path&&*path&&!countdown));
  CHECK(!visible(ui.firmwareRestart));CHECK(!visible(ui.firmwareCancel));
  ui.service();CHECK(enabled(ui.firmwareVerify)==(path&&*path));CHECK(enabled(ui.firmwareInstall)==(path&&*path&&!countdown));
  if(path&&*path){const auto before=Tab5Firmware::installs;ui.act(51);CHECK(Tab5Firmware::installs==before+(countdown?0:1));}
 }
 for(unsigned busy=0;busy<5;++busy) {
  reset();if(busy==0)Tab5Print::active=true;if(busy==1)Tab5Vault::active=true;if(busy==2)Tab5Python::active=true;if(busy==3)Tab5MapPrint::active=true;if(busy==4)executableActive=true;
  FirmwareUI ui;ui.build("/firmware.bin");CHECK(!enabled(ui.firmwareInstall));CHECK(!enabled(ui.firmwareVerify));
  ui.service();CHECK(!enabled(ui.firmwareInstall));CHECK(!enabled(ui.firmwareVerify));
  const auto a=Tab5Firmware::installs,b=Tab5Firmware::inspections,c=Tab5Firmware::restarts;
  ui.act(50);ui.act(51);ui.act(53);CHECK(Tab5Firmware::installs==a);CHECK(Tab5Firmware::inspections==b);CHECK(Tab5Firmware::restarts==c);
 }
 {reset();FirmwareUI ui;ui.build("/firmware.bin");Tab5Firmware::complete=true;Tab5BatteryGuard::current.countdown=true;ui.service();
  CHECK(visible(ui.firmwareRestart));CHECK(!enabled(ui.firmwareRestart));CHECK(!visible(ui.firmwareInstall));
  const auto n=Tab5Firmware::restarts;ui.act(53);CHECK(Tab5Firmware::restarts==n);
  Tab5BatteryGuard::current.countdown=false;ui.service();CHECK(enabled(ui.firmwareRestart));ui.act(53);CHECK(Tab5Firmware::restarts==n+1);
 }
 {reset();FirmwareUI ui;ui.build("/firmware.bin");Tab5Firmware::active=true;ui.service();
  CHECK(!visible(ui.firmwareInstall));CHECK(!visible(ui.firmwareVerify));CHECK(visible(ui.firmwareCancel));CHECK(enabled(ui.firmwareCancel));
  const auto n=Tab5Firmware::cancels;ui.act(52);CHECK(Tab5Firmware::cancels==n+1);
 }
 lv_display_delete(display);lv_deinit();printf("PASS %u checks\n",checks);return 0;
}
