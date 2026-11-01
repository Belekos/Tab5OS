#include <lvgl.h>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include "WriterUsb.h"
static unsigned checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);}}while(0)
namespace writing {
static std::deque<MouseReport> queue;static bool attached=false;
bool usbMouseConnected(){return attached;}
bool usbMousePoll(MouseReport& r){if(queue.empty())return false;r=queue.front();queue.pop_front();return true;}
}
#include "Tab5Mouse.cpp"
static unsigned menus=0;static lv_obj_t* hit=nullptr;static int hx,hy,pages=0;
static void menu(lv_obj_t* o,int x,int y){++menus;hit=o;hx=x;hy=y;}
static void page(int n){pages+=n;}
static void report(uint8_t buttons,int16_t dx=0,int16_t dy=0,int16_t wheel=0,int16_t horizontal=0){writing::MouseReport r;r.buttons=buttons;r.x=dx;r.y=dy;r.wheel=wheel;r.horizontal=horizontal;writing::queue.push_back(r);}
static void pump(){Tab5Mouse::service();lv_obj_update_layout(lv_screen_active());}
int main(){
 lv_init();auto* display=lv_display_create(1280,720);auto* screen=lv_screen_active();lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
 auto* target=lv_button_create(screen);lv_obj_set_pos(target,200,150);lv_obj_set_size(target,300,200);lv_obj_remove_flag(target,LV_OBJ_FLAG_SCROLLABLE);lv_obj_update_layout(screen);
 Tab5Mouse::begin(display);Tab5Mouse::contextMenu(menu);Tab5Mouse::pageScroll(page);writing::attached=true;pump();
 for(unsigned n=0;n<1000;++n){
  unsigned before=menus;report(2);pump();CHECK(menus==before+1);CHECK(hit==target&&hx==320&&hy==240);
  report(2);report(2);pump();CHECK(menus==before+1);CHECK(lv_indev_get_state(Tab5Mouse::input)==LV_INDEV_STATE_RELEASED);
  report(0);pump();CHECK(menus==before+1);
 }
 unsigned before=menus;report(3);pump();CHECK(menus==before);report(2);pump();CHECK(menus==before);report(0);pump();
 Tab5Mouse::Config cfg;cfg.leftHanded=true;Tab5Mouse::configure(cfg);
 report(1);pump();CHECK(menus==before+1);CHECK(lv_indev_get_state(Tab5Mouse::input)==LV_INDEV_STATE_RELEASED);
 report(0);pump();report(2);pump();CHECK(menus==before+1);CHECK(lv_indev_get_state(Tab5Mouse::input)==LV_INDEV_STATE_PRESSED);
 report(3);pump();CHECK(menus==before+1);report(0);pump();
 cfg.leftHanded=false;cfg.speed=25;Tab5Mouse::configure(cfg);int start=Tab5Mouse::x;
 for(unsigned n=0;n<4;++n){report(0,1,0);pump();}CHECK(Tab5Mouse::x==start+1);
 for(unsigned n=0;n<4;++n){report(0,-1,0);pump();}CHECK(Tab5Mouse::x==start);
 cfg.speed=300;Tab5Mouse::configure(cfg);report(0,30000,30000);pump();CHECK(Tab5Mouse::x==1279&&Tab5Mouse::y==719);
 report(0,-30000,-30000);pump();CHECK(Tab5Mouse::x==0&&Tab5Mouse::y==0);
 cfg.speed=100;Tab5Mouse::configure(cfg);report(0,320,240);pump();CHECK(Tab5Mouse::x==320&&Tab5Mouse::y==240);
 report(0,0,0,1);pump();CHECK(pages==1);cfg.natural=true;Tab5Mouse::configure(cfg);report(0,0,0,1);pump();CHECK(pages==0);cfg.natural=false;Tab5Mouse::configure(cfg);
 auto* scroller=lv_obj_create(screen);lv_obj_set_pos(scroller,100,100);lv_obj_set_size(scroller,600,400);lv_obj_set_scroll_dir(scroller,LV_DIR_ALL);
 auto* content=lv_obj_create(scroller);lv_obj_set_size(content,1200,1200);lv_obj_update_layout(screen);lv_obj_scroll_to(scroller,100,100,LV_ANIM_OFF);
 int sy=lv_obj_get_scroll_y(scroller);report(0,0,0,-1);pump();CHECK(lv_obj_get_scroll_y(scroller)>sy);CHECK(pages==0);
 int sx=lv_obj_get_scroll_x(scroller);report(0,0,0,0,1);pump();CHECK(lv_obj_get_scroll_x(scroller)>sx);
 cfg.natural=true;Tab5Mouse::configure(cfg);sy=lv_obj_get_scroll_y(scroller);report(0,0,0,-1);pump();CHECK(lv_obj_get_scroll_y(scroller)<sy);
 lv_obj_delete(scroller);
 // Unplug with stale queued presses must release LVGL, never open a menu.
 report(1);pump();CHECK(lv_indev_get_state(Tab5Mouse::input)==LV_INDEV_STATE_PRESSED);
 before=menus;writing::attached=false;report(1);pump();
 CHECK(lv_indev_get_state(Tab5Mouse::input)==LV_INDEV_STATE_RELEASED);CHECK(Tab5Mouse::buttons==0);CHECK(menus==before);
 report(1);report(2);pump();
 CHECK(lv_indev_get_state(Tab5Mouse::input)==LV_INDEV_STATE_RELEASED);CHECK(menus==before);CHECK(Tab5Mouse::buttons==0);CHECK(writing::queue.empty());
 writing::attached=true;report(0);pump();report(2);pump();CHECK(menus==before+1);report(0);pump();
 lv_deinit();std::printf("%s: %u actual LVGL mouse assertions; %u failures; secondary edges, left-handed, chords, speed, wheel, unplug.\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;
}
