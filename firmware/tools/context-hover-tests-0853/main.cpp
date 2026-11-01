// Actual context menu, focus and mouse service on LVGL. Only USB transport is
// substituted; menu callbacks count effects instead of opening applications.
#include <lvgl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "Tab5ContextMenu.h"
#include "WriterUsb.h"

namespace {
unsigned checks, actions, flushes;
uint32_t now;
bool baseline;
lv_display_t* display;
lv_obj_t* base;
lv_indev_data_t touchData{};
std::vector<uint8_t> framebuffer(800 * 480 * 3), drawbuf(800 * 48 * 3);
constexpr uint32_t selectedColor=0xcbdce8, surfaceColor=0xffffff;
void check(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
}
namespace writing {
std::deque<MouseReport> reports;
bool connected=true;
bool usbMouseConnected(){return connected;}
bool usbMousePoll(MouseReport& report){if(reports.empty())return false;report=reports.front();reports.pop_front();return true;}
}
#include "Tab5Mouse.cpp"

namespace {
void paint(){lv_obj_update_layout(lv_screen_active());lv_obj_update_layout(lv_layer_top());lv_refr_now(display);}
void move(int x,int y,uint8_t buttons=0){
 writing::MouseReport report{};report.x=x-Tab5Mouse::x;report.y=y-Tab5Mouse::y;report.buttons=buttons;
 writing::reports.push_back(report);Tab5Mouse::service();now+=16;paint();
}
lv_point_t center(lv_obj_t* object){lv_area_t a;lv_obj_get_coords(object,&a);return{(a.x1+a.x2)/2,(a.y1+a.y2)/2};}
void moveTo(lv_obj_t* object){const auto p=center(object);move(p.x,p.y);}
void click(lv_obj_t* object){const auto p=center(object);move(p.x,p.y,1);move(p.x,p.y,0);}
uint32_t pixel(int x,int y){const size_t i=(size_t(y)*800+x)*3;return uint32_t(framebuffer[i+2])<<16|uint32_t(framebuffer[i+1])<<8|framebuffer[i];}
uint32_t sample(lv_obj_t* object){lv_area_t a;lv_obj_get_coords(object,&a);return pixel(a.x1+150,(a.y1+a.y2)/2);}
uint32_t color(lv_obj_t* object){return lv_color_to_u32(lv_obj_get_style_bg_color(object,LV_PART_MAIN))&0xffffff;}
lv_obj_t* panel(){auto* o=Tab5ContextMenu::scope();check(o&&lv_obj_is_valid(o),"live menu overlay");return lv_obj_get_child(o,0);}
lv_obj_t* row(unsigned index){return lv_obj_get_child(panel(),index);}
void action(void* amount){actions+=unsigned(reinterpret_cast<uintptr_t>(amount));}
const Tab5ContextMenu::Item items[]={
 {"First","Ctrl+A",action,reinterpret_cast<void*>(1)},
 {"Unavailable","Ctrl+D",action,reinterpret_cast<void*>(100),false},
 {"Second","Ctrl+B",action,reinterpret_cast<void*>(2)},
 {"Third","Ctrl+C",action,reinterpret_cast<void*>(4)}};
void show(){move(740,420);check(Tab5ContextMenu::show(base,32,36,items,4),"menu opens");paint();}
void key(writing::Key key,char letter=0,bool ctrl=false){
 // Exact modality prefix used by the firmware's dispatchKeyboard.
 Tab5Focus.keyboardMode();Tab5UiStyle::keyboardMode();
 writing::KeyEvent event{};event.key=key;event.text[0]=letter;event.ctrl=ctrl;
 check(Tab5ContextMenu::key(event),"context consumes key");paint();
}
void snapshot(const std::string& path){
 FILE* f=std::fopen(path.c_str(),"wb");check(f,"snapshot opens");std::fprintf(f,"P6\n800 480\n255\n");
 for(size_t i=0;i<framebuffer.size();i+=3){const uint8_t rgb[]={framebuffer[i+2],framebuffer[i+1],framebuffer[i]};std::fwrite(rgb,1,3,f);}std::fclose(f);
}
void reopen(void*){
 const Tab5ContextMenu::Item replacement[]={{"Replacement","",action,reinterpret_cast<void*>(8)}};
 check(Tab5ContextMenu::show(base,32,36,replacement,1),"command opens submenu/replacement");
}
}

int main(int argc,char** argv){
 check(argc==3,"mode and output directory supplied");baseline=!std::strcmp(argv[1],"baseline");
 lv_init();lv_tick_set_cb([](){return now;});display=lv_display_create(800,480);
 lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);
 lv_display_set_buffers(display,drawbuf.data(),nullptr,drawbuf.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);
 lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t* a,uint8_t* pixels){
  ++flushes;const size_t bytes=(a->x2-a->x1+1)*3;const size_t stride=lv_draw_buf_width_to_stride(a->x2-a->x1+1,LV_COLOR_FORMAT_RGB888);
  for(int y=a->y1;y<=a->y2;++y)std::memcpy(framebuffer.data()+(size_t(y)*800+a->x1)*3,pixels+size_t(y-a->y1)*stride,bytes);
  lv_display_flush_ready(d);
 });
 const uint32_t palette[]={0xf4f4f0,surfaceColor,0xe8e8e4,0x777777,0x161616,0x666666,0x365b78,0x161616,selectedColor};
 Tab5UiStyle::configure(palette,7);Tab5ContextMenu::configure([](){return &lv_font_montserrat_22;},[](const char* text){return text;});
 Tab5Focus.begin(lv_layer_top(),&lv_font_montserrat_22);Tab5Focus.theme(lv_color_hex(selectedColor),lv_color_hex(0x161616),lv_color_hex(surfaceColor));
 base=lv_obj_create(lv_screen_active());lv_obj_set_size(base,800,480);lv_obj_set_pos(base,0,0);lv_obj_remove_flag(base,LV_OBJ_FLAG_SCROLLABLE);
 auto* ownerButton=lv_button_create(base);lv_obj_set_size(ownerButton,100,40);Tab5UiStyle::selection(ownerButton);Tab5Focus.focusObject(ownerButton);
 Tab5Mouse::begin(display);Tab5Mouse::service();
 auto* touch=lv_indev_create();lv_indev_set_type(touch,LV_INDEV_TYPE_POINTER);lv_indev_set_display(touch,display);lv_indev_set_mode(touch,LV_INDEV_MODE_EVENT);
 lv_indev_set_read_cb(touch,[](lv_indev_t*,lv_indev_data_t* data){*data=touchData;});Tab5UiStyle::touchInput(touch);
 Tab5Focus.keyboardMode();show();key(writing::Key::Home);
 auto* first=row(0);auto* third=row(3);check(color(first)==selectedColor,"keyboard initially highlights first enabled row");
 const auto before=sample(third);moveTo(third);const auto after=sample(third);
 check(actions==0,"hover does not invoke any action");check(color(first)!=selectedColor,"mouse clears stale keyboard highlight");
 check(lv_obj_has_state(third,LV_STATE_HOVERED),"native hover reaches menu button");
 if(baseline){
  check(before==surfaceColor&&after==surfaceColor,"baseline hover leaves rendered row unhighlighted");
  check(color(third)==surfaceColor,"baseline missing HOVERED style reproduced");
  snapshot(std::string(argv[2])+"/baseline-hover.ppm");Tab5ContextMenu::close();
 }else{
  check(before==surfaceColor&&after==selectedColor,"rendered hovered row uses configured selection color");
  check(Tab5Focus.focused()==first,"mouse hover preserves logical keyboard target");
  snapshot(std::string(argv[2])+"/current-hover.ppm");
  move(700,420);check(color(third)==surfaceColor,"leaving row removes highlight");check(Tab5Focus.focused()==first,"leaving menu rows does not clear logical focus");
  key(writing::Key::Down);check(Tab5Focus.focused()==row(2),"keyboard resumes from saved row and skips disabled row");check(color(row(2))==selectedColor,"resumed keyboard highlight visible");
  moveTo(row(3));check(color(row(2))==surfaceColor&&color(row(3))==selectedColor,"mouse reclaims highlight from keyboard");
  key(writing::Key::Enter);check(actions==0,"keyboard action remains deferred");Tab5ContextMenu::service();check(actions==2&&!Tab5ContextMenu::scope(),"Enter invokes logical keyboard choice, not last hovered row");
  show();moveTo(row(1));click(row(1));Tab5ContextMenu::service();check(actions==2&&Tab5ContextMenu::scope(),"disabled mouse choice cannot execute");
  click(row(3));check(actions==2&&Tab5ContextMenu::scope(),"real mouse click queues once");Tab5ContextMenu::service();check(actions==6&&!Tab5ContextMenu::scope(),"mouse click executes once after close");
  show();key(writing::Key::Text,'d',true);Tab5ContextMenu::service();check(actions==6&&Tab5ContextMenu::scope(),"disabled shortcut cannot execute");
  key(writing::Key::Text,'a',true);Tab5ContextMenu::service();check(actions==7&&!Tab5ContextMenu::scope(),"enabled keyboard shortcut remains functional");
  show();key(writing::Key::Escape);Tab5ContextMenu::service();check(actions==7&&!Tab5ContextMenu::scope(),"Escape cancels without command");
  show();const auto p=center(row(2));touchData.point=p;touchData.state=LV_INDEV_STATE_PRESSED;lv_indev_read(touch);paint();
  check(lv_obj_has_state(row(2),LV_STATE_PRESSED),"touch keeps pressed state");check(!lv_obj_has_state(row(2),LV_STATE_HOVERED),"touch does not leave mouse-hover state");
  touchData.state=LV_INDEV_STATE_RELEASED;lv_indev_read(touch);paint();check(actions==7,"touch release queues callback only");
  Tab5ContextMenu::service();check(actions==9&&!Tab5ContextMenu::scope(),"touch invokes exactly one command");
  const Tab5ContextMenu::Item rootMenu[]={{"Submenu","",reopen}};check(Tab5ContextMenu::show(base,32,36,rootMenu,1),"open submenu source");paint();click(row(0));Tab5ContextMenu::service();paint();
  check(Tab5ContextMenu::scope()&&std::strcmp(lv_label_get_text(lv_obj_get_child(row(0),0)),"Replacement")==0,"replacement menu survives prior cleanup");
  click(row(0));Tab5ContextMenu::service();check(actions==17&&!Tab5ContextMenu::scope(),"replacement action executes once");
  show();click(row(0));lv_obj_delete(base);base=nullptr;Tab5ContextMenu::service();move(700,420);check(actions==17&&!Tab5ContextMenu::scope(),"owner deletion cancels queued command and hover references");
  const auto idleFlushes=flushes;for(unsigned i=0;i<50;++i){check(!Tab5Mouse::service(),"idle mouse has no activity");paint();}check(flushes==idleFlushes,"idle service causes no refresh");
 }
 if(base){lv_obj_delete(base);base=nullptr;}
 std::printf("{\"baseline\":%s,\"checks\":%u,\"actions\":%u,\"hoverBefore\":%u,\"hoverAfter\":%u,\"passed\":true}\n",baseline?"true":"false",checks,actions,before,after);
}
