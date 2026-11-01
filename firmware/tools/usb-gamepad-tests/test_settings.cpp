#include <lvgl.h>
#include "WriterGamepad.h"
#include "WriterKeys.h"
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using String=std::string;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);std::exit(1);}}while(0)
enum class Page{Home,Settings};static Page activePage=Page::Settings;
static unsigned WORK_W=1168;
static uint32_t now=0;static uint32_t millis(){return now;}
static const char* T(const char* s){return s;}
static std::string toasted;
static void toast(const char* s){toasted=s;}
namespace writing {
static GamepadInfo live;static unsigned saves=0;
bool usbGamepadInfo(GamepadInfo& o){o=live;return o.identity!=0;}
bool usbGamepadMap(uint32_t id,const uint8_t m[8]){CHECK(id==live.identity);std::memcpy(live.mapping,m,8);++saves;return true;}
}
struct Focus {lv_obj_t* target=nullptr;void focusObject(lv_obj_t* p){target=p;}}Tab5Focus;
namespace Tab5Shell {void width(lv_obj_t* p,int delta){lv_obj_set_width(p,WORK_W+delta);}}
static void setButtonEnabled(lv_obj_t* p,bool e){if(e)lv_obj_remove_state(p,LV_STATE_DISABLED);else lv_obj_add_state(p,LV_STATE_DISABLED);}
static lv_obj_t* makeButton(lv_obj_t* p,const char* t,int w,int h){auto* o=lv_button_create(p);lv_obj_set_size(o,w,h);auto* l=lv_label_create(o);lv_label_set_text(l,t);lv_obj_center(l);return o;}
static lv_obj_t* makeSettingsCard(lv_obj_t* p,int x,int y,int w,int h,const char* t){auto* o=lv_obj_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_pad_all(o,0,0);auto* l=lv_label_create(o);lv_label_set_text(l,t);return o;}
static lv_obj_t* makeSettingsText(lv_obj_t* p,const char* t,int x,int y,int w,bool){auto* l=lv_label_create(p);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,w);lv_label_set_text(l,t);return l;}
#include "Tab5GamepadSettings.inc"
static void tick(){now+=100;serviceGamepadSettings();lv_obj_update_layout(lv_screen_active());}
int main(){
 lv_init();auto* display=lv_display_create(1280,720);(void)display;
 auto* parent=lv_obj_create(lv_screen_active());lv_obj_set_size(parent,1168,640);
 createGamepadSettings(parent,0);tick();CHECK(lv_obj_has_state(settingsPadConfigure,LV_STATE_DISABLED));
 CHECK(!std::strcmp(lv_label_get_text(settingsPadLabel),"Branchez une manette USB"));
 writing::live.identity=1;writing::live.vendor=0x2dc8;writing::live.product=0x6001;writing::live.xinput=true;tick();
 CHECK(!lv_obj_has_state(settingsPadConfigure,LV_STATE_DISABLED));CHECK(std::strstr(lv_label_get_text(settingsPadLabel),"SN30 Pro"));CHECK(std::strstr(lv_label_get_text(settingsPadLabel),"Aucune commande reçue"));
 writing::live.rejected=1;tick();CHECK(std::strstr(lv_label_get_text(settingsPadLabel),"Commandes non reconnues"));
 writing::live.reports=1;writing::live.buttons=1;writing::live.directions=5;tick();
 CHECK(std::strstr(lv_label_get_text(settingsPadLabel),"A"));CHECK(!std::strstr(lv_label_get_text(settingsPadLabel),"non reconnues"));
 CHECK(lv_obj_get_y(settingsPadLabel)+lv_obj_get_height(settingsPadLabel)<lv_obj_get_y(settingsPadConfigure));
 lv_obj_send_event(settingsPadConfigure,LV_EVENT_CLICKED,nullptr);tick();CHECK(settingsPadStep==0);CHECK(!settingsPadReleased);CHECK(Tab5Focus.target==settingsPadCancel);
 CHECK(!lv_obj_has_flag(settingsPadCancel,LV_OBJ_FLAG_HIDDEN));CHECK(lv_obj_has_state(settingsPadConfigure,LV_STATE_DISABLED));
 // Holding the initiating button does not assign it to A automatically.
 tick();CHECK(settingsPadStep==0);
 for(unsigned i=0;i<8;++i){writing::live.buttons=0;tick();CHECK(settingsPadReleased);writing::live.buttons=1u<<(7-i);tick();CHECK(settingsPadStep==(i==7?-1:int(i+1)));tick();if(i<7)CHECK(settingsPadStep==int(i+1));}
 CHECK(writing::saves==1);CHECK(toasted=="Commandes enregistrées");CHECK(lv_obj_has_flag(settingsPadCancel,LV_OBJ_FLAG_HIDDEN));
 for(unsigned i=0;i<8;++i)CHECK(writing::live.mapping[i]==8-i);
 lv_obj_send_event(settingsPadConfigure,LV_EVENT_CLICKED,nullptr);tick();writing::KeyEvent key;key.key=writing::Key::Escape;
 CHECK(gamepadSettingsKey(key));CHECK(settingsPadStep==-1);CHECK(writing::saves==1);
 lv_obj_send_event(settingsPadConfigure,LV_EVENT_CLICKED,nullptr);tick();writing::live.identity=0;tick();CHECK(settingsPadStep==-1);CHECK(std::strstr(lv_label_get_text(settingsPadLabel),"déconnectée"));
 writing::live.identity=2;tick();lv_obj_send_event(settingsPadConfigure,LV_EVENT_CLICKED,nullptr);tick();activePage=Page::Home;tick();CHECK(settingsPadStep==-1);CHECK(!gamepadSettingsKey(key));
 activePage=Page::Settings;tick();lv_obj_send_event(settingsPadConfigure,LV_EVENT_CLICKED,nullptr);tick();lv_obj_add_flag(parent,LV_OBJ_FLAG_HIDDEN);tick();CHECK(settingsPadStep==-1);CHECK(!gamepadSettingsKey(key));
 lv_obj_delete(parent);lv_deinit();std::printf("PASS: %u actual LVGL settings assertions, live controls outside F1, 8 mappings, held-button guard, cancel/disconnect/page exit.\n",checks);
}
