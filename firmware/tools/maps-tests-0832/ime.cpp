#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <lvgl.h>
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5Ime.h"
class String:public std::string {public:using std::string::string;String(const std::string& x):std::string(x){}String(size_t x):std::string(std::to_string(x)){};};
static unsigned checks=0;
static void check(bool ok,const char* s){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",s);exit(1);}}
static const char* T(const char* s){return s;}
static lv_color_t C_TEXT(){return lv_color_hex(0xeeeeee);}
static lv_color_t C_BORDER(){return lv_color_hex(0x556677);}
static lv_color_t C_SURFACE(){return lv_color_hex(0x1f252b);}
static lv_color_t C_MUTED(){return lv_color_hex(0x778899);}
static lv_color_t C_RAISED(){return C_SURFACE();}
static lv_color_t C_BG(){return lv_color_hex(0x101215);}
static lv_color_t C_ACTIVE(){return lv_color_hex(0x378abb);}
static lv_color_t C_SELECTED(){return C_ACTIVE();}
static lv_font_t* testFont=nullptr;
static const lv_font_t* uiFont(){return testFont?testFont:LV_FONT_DEFAULT;}
static struct{int cornerRadius=12;}settings;
static constexpr int SCREEN_W=1280,SCREEN_H=720;
static lv_obj_t *root=nullptr,*scope=nullptr,*keyboardPanel=nullptr,*keyboardTarget=nullptr;
static lv_obj_t* keyboardScope(){return scope?scope:root;}
/* BUTTON */
static lv_obj_t* makeIconButton(lv_obj_t* p,const char* text,int w,int h){return makeButton(p,text,w,h);}
enum class Page{Map,Writing};static Page activePage=Page::Map;
static struct Writing {bool isInput(lv_obj_t*){return false;}bool editing(){return false;}lv_obj_t* object(){return nullptr;}void insert(const char*){}} Tab5Writing;
static struct {bool isInput(lv_obj_t*){return false;}void insert(const char*){}} Tab5Tools;
namespace Tab5Alarm {static bool visible(){return false;}}
namespace Tab5NativeView {static bool visible(){return false;}}
static struct {String id="fr-FR";String selectedId(){return id;}} Tab5Keyboards;
static struct {bool availableFor(const String&){return true;}} Tab5Cjk;
struct Tab5ImeDictionary {static constexpr size_t PAGE_SIZE=6;struct Page{String words[PAGE_SIZE];size_t count=0;uint32_t total=0;bool available=false;};Page lookup(tab5ime::Mode,const char*,uint32_t){Page p;p.available=true;return p;}};
static Tab5ImeDictionary Tab5ImeWords;
namespace Tab5NumericKeyboard {
/* NUMERIC */
}
/* IME */
static bool failEventRegistration=false;
extern "C" lv_event_dsc_t* __real_lv_obj_add_event_cb(lv_obj_t*,lv_event_cb_t,lv_event_code_t,void*);
extern "C" lv_event_dsc_t* __wrap_lv_obj_add_event_cb(lv_obj_t* object,lv_event_cb_t cb,lv_event_code_t code,void* data){
 if(failEventRegistration){failEventRegistration=false;return nullptr;}
 return __real_lv_obj_add_event_cb(object,cb,code,data);
}
static void repaint(lv_display_t* display){lv_obj_update_layout(root);lv_tick_inc(20);lv_refr_now(display);}
static unsigned watcherCount(lv_obj_t* object){return lv_obj_get_event_count(object);}
int main(int argc,char** argv){
 check(argc==2,"font path supplied");std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> fontBytes{std::istreambuf_iterator<char>(file),{}};check(!fontBytes.empty(),"release SD font loaded");
 lv_init();auto* display=lv_display_create(SCREEN_W,SCREEN_H);static uint8_t pixels[SCREEN_W*SCREEN_H*2];lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});
 testFont=lv_tiny_ttf_create_data_ex(fontBytes.data(),fontBytes.size(),20,LV_FONT_KERNING_NORMAL,256);check(testFont,"release font valid");testFont->fallback=&lv_font_montserrat_20;
 root=lv_screen_active();lv_display_set_theme(display,lv_theme_mono_init(display,true,uiFont()));Tab5Focus.begin(root,uiFont());keyboardPanel=lv_obj_create(root);
 const char* names[]={"Saint Nazaire","Le Havre","New York","La Rochelle"};
 for(unsigned cycle=0;cycle<400;++cycle){
  Tab5Keyboards.id="fr-FR";scope=lv_obj_create(root);auto* input=lv_textarea_create(scope);keyboardTarget=input;
  lv_textarea_set_one_line(input,true);lv_textarea_set_max_length(input,96);lv_obj_set_style_text_font(input,uiFont(),0);Tab5Focus.editObject(input);
  const char* name=names[cycle%4];
  for(const char* p=name;*p;++p){writing::KeyEvent key{};key.key=writing::Key::Text;key.text[0]=*p;check(!imePhysical(key,input),"Latin keyboard falls through IME");check(Tab5TextInput::key(input,key),"search character delivered");}
  check(imeInput==input,"IME tracks search field even in Latin mode");
  lv_obj_add_event_cb(input,[](lv_event_t*){auto* old=scope;scope=nullptr;keyboardTarget=nullptr;lv_obj_delete(old);},LV_EVENT_READY,nullptr);
  writing::KeyEvent enter{};enter.key=writing::Key::Enter;
  check(!imePhysical(enter,input),"Latin Enter falls through IME");check(Tab5Focus.key(enter,scope),"search submit closes real LVGL dialog");
  // The firmware calls this every UI loop, before the next search opens.
  imeService();
  check(imeInput==nullptr,"deleted search input forgotten");check(imeComposer.empty(),"deleted search clears composition");
  repaint(display);
 }
 for(const char* language:{"zh-CN","ja-JP","ko-KR"})for(unsigned cycle=0;cycle<100;++cycle){
  Tab5Keyboards.id=language;scope=lv_obj_create(root);auto* first=lv_textarea_create(scope);auto* second=lv_textarea_create(scope);
  const unsigned events=watcherCount(first);imeSync(first);const unsigned watched=watcherCount(first);
  check(watched==events+1,"one input lifetime watcher registered");for(unsigned n=0;n<30;++n)imeSync(first);check(watcherCount(first)==watched,"repeated sync does not accumulate watchers");
  imeComposer.feed('n');imeComposer.feed('i');check(!imeComposer.empty(),"CJK preedit created");imeMount(keyboardPanel);imeRender();
  imeSync(second);check(imeInput==second&&imeComposer.empty(),"input switch cancels uncommitted old text");check(watcherCount(first)==events,"old target watcher removed on switch");
  lv_obj_delete(first);check(imeInput==second,"deleting previous field leaves new target intact");
  imeComposer.feed('n');auto* old=scope;scope=nullptr;lv_obj_delete(old);imeService();
  check(imeInput==nullptr&&imeComposer.empty()&&imePage.count==0,"delete clears target, preedit and candidate page");
  check(imePanel&&lv_obj_has_flag(imePanel,LV_OBJ_FLAG_HIDDEN),"floating or keyboard preedit hidden after field deletion");
  lv_obj_clean(keyboardPanel);check(!imePanel&&!imeCandidates&&!imePreview,"IME panel pointers cleared with parent");
 }
 // Explicit detachment (app no longer owns focus) unregisters the old watcher.
 {scope=lv_obj_create(root);auto* input=lv_textarea_create(scope);const auto before=watcherCount(input);imeSync(input);imeSync(nullptr);check(watcherCount(input)==before&&!imeInput,"explicit detach releases watcher");auto* old=scope;scope=nullptr;lv_obj_delete(old);imeService();}
 // Simulate LVGL declining only the input watcher allocation. The IME must
 // decline ownership and let normal input continue, never retain an untracked pointer.
 {Tab5Keyboards.id="zh-CN";scope=lv_obj_create(root);auto* input=lv_textarea_create(scope);
  failEventRegistration=true;imeSync(input);check(!failEventRegistration&&!imeInput,"failed watcher allocation never retains input");
  writing::KeyEvent key{};key.key=writing::Key::Text;key.text[0]='n';failEventRegistration=true;
  check(!imePhysical(key,input)&&!imeInput,"failed ownership declines composition safely");
  auto* old=scope;scope=nullptr;lv_obj_delete(old);imeService();check(!imeInput,"failed watcher field may be destroyed safely");}
 Tab5Focus.clear();lv_obj_clean(root);lv_tiny_ttf_destroy(testFont);testFont=nullptr;lv_deinit();
 printf("PASS %u checks, 400 repeated Latin search dialogs, 300 CJK input-switch/deletion cycles, actual IME/LVGL/Focus/TextInput, ASan+UBSan\n",checks);
}
