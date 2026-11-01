#include <lvgl.h>
#include <algorithm>
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Tab5BatteryGuard.h"
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static bool longLanguage=false,lightTheme=false;
static const char* T(const char* text) {
 if(longLanguage&&strstr(text,"%lu"))return "Kritischer Akkustand: Das Tablet wird in %lu Sekunden ausgeschaltet. Schließen Sie die Stromversorgung an und speichern Sie Ihre aktuelle Arbeit, solange dies noch möglich ist.";
 if(longLanguage&&!strcmp(text,"Copie de secours impossible : enregistrez votre travail."))return "Die Sicherungskopie konnte nicht gespeichert werden. Speichern Sie Ihre aktuelle Arbeit.";
 return text;
}
static lv_color_t C_RAISED(){return lv_color_hex(lightTheme?0xfafafa:0x222222);}
static lv_color_t C_ACTIVE(){return lv_color_hex(lightTheme?0x005599:0x66bbff);}
static lv_color_t C_TEXT(){return lv_color_hex(lightTheme?0x111111:0xffffff);}
static lv_color_t C_MUTED(){return lv_color_hex(lightTheme?0x333333:0xbbbbbb);}
static const lv_font_t* richFontFor(uint16_t,uint8_t){return &lv_font_montserrat_20;}
static void setLabelIfChanged(lv_obj_t* label,const char* text){if(strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);}
static unsigned toasts=0;
static void toast(const char*,uint32_t){++toasts;}
static struct ScreenIdle {unsigned wakes=0;void wake(uint32_t){++wakes;}} screenIdle;
struct Tab5WritingApp {
 enum class RecoverySaveState:uint8_t{Idle,Pending,Saved,Failed};
 RecoverySaveState state=RecoverySaveState::Idle;
 unsigned requests=0,cancels=0;
 void requestRecoverySave(){++requests;state=RecoverySaveState::Pending;}
 void cancelRecoverySave(){++cancels;if(state==RecoverySaveState::Pending)state=RecoverySaveState::Failed;}
 RecoverySaveState recoverySaveState()const{return state;}
} Tab5Writing;
namespace Tab5Firmware{static unsigned cancelled;void cancel(){++cancelled;}}
namespace Tab5GitView{static unsigned cancelled;void cancel(){++cancelled;}}
namespace Tab5Terminal{static unsigned cancelled;void stop(){++cancelled;}}
namespace Tab5Vault{static unsigned cancelled;void cancel(){++cancelled;}}
namespace Tab5FileOperation{static unsigned cancelled;void cancel(){++cancelled;}}
namespace Tab5BatteryGuard {
 static Result next;static bool confirmed=false;static unsigned attempts=0,off=0;
 Result service(uint32_t){return next;}
 const Result& status(){return next;}
 bool powerOffIfStillCritical(uint32_t){++attempts;if(confirmed)++off;return confirmed;}
}
#include "Tab5BatteryUi.inc"
static void event(Tab5BatteryGuard::Event e,unsigned seconds=0){Tab5BatteryGuard::next.event=e;Tab5BatteryGuard::next.seconds=seconds;serviceBatteryProtection(10000+seconds);}
static void geometry(lv_display_t* display,int width,bool translated,bool light){
 longLanguage=translated;lightTheme=light;lv_display_set_resolution(display,width,720);
 showBatteryProtectionNotice(29);lv_obj_update_layout(lv_screen_active());lv_obj_update_layout(lv_layer_top());
 CHECK(batteryGuardBanner);CHECK(lv_obj_get_x(batteryGuardBanner)>=0);
 CHECK(lv_obj_get_x(batteryGuardBanner)+lv_obj_get_width(batteryGuardBanner)<=width);
 CHECK(lv_obj_get_y(batteryGuardBanner)==64);CHECK(lv_obj_get_height(batteryGuardBanner)<360);
 CHECK(!lv_obj_has_flag(batteryGuardBanner,LV_OBJ_FLAG_CLICKABLE));
 CHECK(!lv_obj_has_flag(batteryGuardBanner,LV_OBJ_FLAG_SCROLLABLE));
 CHECK(lv_color_eq(lv_obj_get_style_bg_color(batteryGuardBanner,LV_PART_MAIN),C_RAISED()));
 CHECK(lv_color_eq(lv_obj_get_style_text_color(batteryGuardMessage,LV_PART_MAIN),C_TEXT()));
 for(auto* label:{batteryGuardMessage,batteryGuardRecovery}) {
  lv_area_t a,b;lv_obj_get_coords(label,&a);lv_obj_get_coords(batteryGuardBanner,&b);
  CHECK(a.x1>=b.x1);CHECK(a.x2<=b.x2);CHECK(a.y1>=b.y1);CHECK(a.y2<=b.y2);
 }
}
int main(){
 lv_init();auto* display=lv_display_create(1280,720);
 static uint8_t buffer[1280*12*4];lv_display_set_buffers(display,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
 lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});
 using E=Tab5BatteryGuard::Event;using S=Tab5WritingApp::RecoverySaveState;
 event(E::Low);CHECK(toasts==1);CHECK(Tab5Writing.requests==0);CHECK(!batteryGuardBanner);
 event(E::Countdown,30);CHECK(Tab5Writing.requests==1);CHECK(batteryGuardCycle);
 CHECK(Tab5Firmware::cancelled==1);CHECK(Tab5GitView::cancelled==1);CHECK(Tab5Terminal::cancelled==1);
 CHECK(Tab5Vault::cancelled==1);CHECK(Tab5FileOperation::cancelled==1);
 event(E::Countdown,29);CHECK(Tab5Writing.requests==1);
 for(auto state:{S::Pending,S::Saved,S::Failed}) {
  Tab5Writing.state=state;
  for(int width:{360,720,1216,1280})for(bool translated:{false,true})for(bool light:{false,true})geometry(display,width,translated,light);
 }
 event(E::Cancelled);CHECK(!batteryGuardBanner);CHECK(!batteryGuardCycle);CHECK(Tab5Writing.cancels==1);CHECK(toasts==2);
 event(E::None);CHECK(!batteryGuardBanner);CHECK(Tab5Writing.requests==1);
 event(E::Countdown,5);CHECK(Tab5Writing.requests==2);CHECK(Tab5Firmware::cancelled==2);
 Tab5BatteryGuard::confirmed=false;event(E::Shutdown);CHECK(Tab5BatteryGuard::attempts==1);CHECK(Tab5BatteryGuard::off==0);
 CHECK(!batteryGuardBanner);CHECK(!batteryGuardCycle);CHECK(Tab5Writing.cancels==2);
 event(E::Countdown,5);Tab5Writing.state=S::Saved;Tab5BatteryGuard::confirmed=true;event(E::Shutdown);
 CHECK(Tab5BatteryGuard::off==1);CHECK(Tab5Writing.cancels==3);CHECK(Tab5Writing.state==S::Saved);
 closeBatteryProtectionNotice();batteryGuardCycle=false;
 for(unsigned i=0;i<200;++i){event(E::Countdown,30);event(E::Countdown,29);event(E::Cancelled);CHECK(!batteryGuardBanner);CHECK(!batteryGuardCycle);}
 CHECK(Tab5Writing.requests==203);CHECK(Tab5BatteryGuard::off==1);
 lv_display_delete(display);lv_deinit();printf("PASS %u checks\n",checks);return 0;
}
