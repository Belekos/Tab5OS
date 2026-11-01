#include <lvgl.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include "Tab5Writing.h"
#include "Tab5Mouse.h"
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "Tab5TextInput.h"
#include "WriterDocument.h"
#include "WriterUsb.h"
uint32_t testMillis=0;
static unsigned checks=0,actions=0;
static lv_font_t* osFont=nullptr;static std::vector<uint8_t> fontData;
static void check(bool ok,const char* why){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static std::deque<writing::MouseReport> reports;
namespace writing {bool usbConnected(){return true;}bool usbMouseConnected(){return true;}bool usbMousePoll(MouseReport& r){if(reports.empty())return false;r=reports.front();reports.pop_front();return true;}}
struct Impl {
 static Impl* instance;
 WritingHooks hooks;
 lv_style_t buttonStyle{},panelStyle{},selectedStyle{},mutedStyle{};
 lv_obj_t *root=nullptr,*toolbar=nullptr,*menuShade=nullptr,*menuPanel=nullptr,*menuOrigin=nullptr,*boldButton=nullptr,*italicButton=nullptr;
 lv_obj_t* menuButtons[8]{};
 int Width=1216,height=672,openMenu=0;
 bool menuWasEditing=false,documentSelection=false,serialVisible=false,split=false,consoleVisible=false;
 size_t selectionAnchor=0,selectionCaret=0;
 writing::Document document;
 std::string path="/Documents/sample.txt";
 struct Save {bool busy(){return false;}} saveJob;
 struct View {bool editing=false;size_t caret=0,anchor=0;void blur(){editing=false;}void wakeCaret(){}} view;
 /* enum Action { */
 const char* tr(const char* p){return p;}
 const char* actionName(int){return "action";}
 const lv_font_t* font(int=20){return osFont;}
 uint32_t color(WritingColor c){return c==WritingColor::Selection?Tab5UiStyle::color(8):c==WritingColor::Text?Tab5UiStyle::color(4):c==WritingColor::Muted?Tab5UiStyle::color(5):Tab5UiStyle::color(1);}
 void caption(lv_obj_t* label,const char* text){lv_label_set_text(label,text);}
 void fillDocumentsMenu(){}
 bool isJson(){return false;}
 bool canRunPython(){return false;}
 void syncStyle(){}
 void act(int action){if(action>=MenuFile&&action<=MenuCode){toggleMenu(action);return;}++actions;closeMenu(false);}
 /* void panel( */
 /* lv_obj_t* button( */
 /* void closeMenu( */
 /* void toggleMenu( */
 void init(lv_obj_t* screen){
  instance=this;for(auto* s:{&buttonStyle,&panelStyle,&selectedStyle,&mutedStyle})lv_style_init(s);
  lv_style_set_bg_color(&buttonStyle,lv_color_hex(color(WritingColor::Surface)));
  lv_style_set_text_color(&buttonStyle,lv_color_hex(color(WritingColor::Text)));
  lv_style_set_bg_color(&selectedStyle,lv_color_hex(color(WritingColor::Selection)));
  lv_style_set_text_color(&selectedStyle,lv_color_hex(color(WritingColor::Text)));
  root=lv_obj_create(screen);panel(root);lv_obj_set_pos(root,64,48);lv_obj_set_size(root,Width,height);
  toolbar=lv_obj_create(root);panel(toolbar);lv_obj_set_size(toolbar,Width,40);
  const char* titles[]={"Fichier","Édition","Insertion","Format","Affichage","Aide","Documents","Code"};
  for(int i=0;i<8;++i){menuButtons[i]=button(toolbar,titles[i],MenuFile+i,130);lv_obj_set_x(menuButtons[i],i*134);}
 }
};
Impl* Impl::instance=nullptr;
static lv_display_t* display;static lv_obj_t* screen;
static std::vector<unsigned char> pixels(1280*720*3);
static void pump(){testMillis+=20;lv_tick_inc(20);lv_timer_handler();lv_obj_update_layout(screen);}
static void mouse(int x,int y,int buttons=0){static int px=320,py=240;writing::MouseReport r;r.x=x-px;r.y=y-py;r.buttons=buttons;reports.push_back(r);px=x;py=y;Tab5Mouse::service();check(reports.empty(),"production mouse service consumes report");pump();}
static lv_point_t center(lv_obj_t* o){lv_area_t a;lv_obj_get_coords(o,&a);return {(a.x1+a.x2)/2,(a.y1+a.y2)/2};}
static void click(lv_obj_t* o){auto p=center(o);mouse(p.x,p.y,1);mouse(p.x,p.y);}
static void snapshot(const std::string& path){lv_obj_invalidate(screen);lv_refr_now(display);FILE* f=fopen((path+".ppm").c_str(),"wb");check(f,"capture opens");fprintf(f,"P6\n1280 720\n255\n");for(size_t i=0;i<pixels.size();i+=3){unsigned char rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,f);}fclose(f);}
static bool selection(lv_obj_t* b){return lv_color_eq(lv_obj_get_style_bg_color(b,LV_PART_MAIN),lv_color_hex(Tab5UiStyle::color(8)));}
static void key(writing::Key k,lv_obj_t* scope){
#ifndef BASELINE
 Tab5Focus.keyboardMode();Tab5UiStyle::keyboardMode();
#endif
 writing::KeyEvent e{};e.key=k;check(Tab5Focus.key(e,scope),"keyboard menu route consumes key");pump();}
int main(int argc,char**argv){check(argc==3,"output and shipped OS font supplied");std::ifstream fontFile(argv[2],std::ios::binary);fontData.assign(std::istreambuf_iterator<char>(fontFile),{});lv_init();osFont=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),20);check(osFont,"shipped multilingual OS font loads");osFont->fallback=&lv_font_montserrat_20;display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});screen=lv_obj_create(nullptr);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);lv_screen_load(screen);lv_obj_set_style_text_font(screen,osFont,0);Tab5Focus.begin(screen,osFont);Tab5Mouse::begin(display);
 Impl app;app.init(screen);pump();click(app.menuButtons[0]);check(app.menuPanel&&lv_obj_get_child_count(app.menuPanel)==9,"actual File menu constructs nine command buttons");auto* first=lv_obj_get_child(app.menuPanel,0);auto* third=lv_obj_get_child(app.menuPanel,2);auto p=center(third);mouse(p.x,p.y);check(lv_obj_has_state(third,LV_STATE_HOVERED),"LVGL receives native mouse hover on Writing command");check(actions==0,"hover invokes no Writing action");
#ifdef BASELINE
 check(!selection(third),"baseline reproduces absent selected-colour hover in Writing button");
#else
 check(selection(third),"Writing hovered command uses selection palette");check(!lv_obj_has_state(first,LV_STATE_FOCUS_KEY),"mouse hides preselected keyboard focus without changing focus target");check(Tab5Focus.focused()==first,"mouse does not move logical keyboard focus");
#endif
 snapshot(std::string(argv[1])+"-file-hover");mouse(1240,710);check(!lv_obj_has_state(third,LV_STATE_HOVERED),"leaving Writing menu clears native hover state");
#ifndef BASELINE
 check(!selection(third),"leaving Writing command clears its hover colour");key(writing::Key::Down,app.menuPanel);check(Tab5Focus.focused()==lv_obj_get_child(app.menuPanel,1),"keyboard resumes from its remembered first command");key(writing::Key::Enter,app.menuPanel);check(actions==1&&!app.menuPanel,"keyboard command still activates exactly once");
 for(int n=0;n<3;++n){click(app.menuButtons[0]);auto* b=lv_obj_get_child(app.menuPanel,2);click(b);check(actions==unsigned(n+2)&&!app.menuPanel,"mouse repeated command activates once and closes menu");}
 uint32_t dark[]={0x101418,0x202428,0x30363c,0x777777,0xf1f3f5,0x999999,0x3288cf,0xffffff,0x334455};Tab5UiStyle::configure(dark,1);lv_style_set_bg_color(&app.selectedStyle,lv_color_hex(app.color(WritingColor::Selection)));lv_style_set_bg_color(&app.buttonStyle,lv_color_hex(app.color(WritingColor::Surface)));lv_style_set_text_color(&app.buttonStyle,lv_color_hex(app.color(WritingColor::Text)));lv_style_set_text_color(&app.selectedStyle,lv_color_hex(app.color(WritingColor::Text)));lv_style_set_text_color(&app.mutedStyle,lv_color_hex(app.color(WritingColor::Muted)));click(app.menuButtons[0]);third=lv_obj_get_child(app.menuPanel,2);p=center(third);mouse(p.x,p.y);check(selection(third),"Writing hover follows changed dark palette");snapshot(std::string(argv[1])+"-dark-hover");
#endif
 app.closeMenu(false);Tab5Focus.clear();lv_obj_delete(screen);lv_display_delete(display);for(auto* s:{&app.buttonStyle,&app.panelStyle,&app.selectedStyle,&app.mutedStyle})lv_style_reset(s);lv_tiny_ttf_destroy(osFont);lv_deinit();printf("{\"checks\":%u,\"actions\":%u,\"passed\":true}\n",checks,actions);}
