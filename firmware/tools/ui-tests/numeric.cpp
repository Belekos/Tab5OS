#include <lvgl.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
static unsigned checks=0,ready=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Tab5KeyDef {std::string label,value,accents;uint8_t grow=1;};
#include "numeric_production.inc"
namespace writing {enum class Key{Backspace,Enter,Left,Right};struct KeyEvent{Key key;};}
enum class KeyboardMode{Lower,Upper,Symbols};static KeyboardMode keyboardMode;
static lv_obj_t* keyboardTarget=nullptr;static bool keyboardVisible=true;
struct InputBoundary {bool isInput(lv_obj_t*){return false;}void touchKey(writing::Key){std::abort();}}Tab5Writing,Tab5Tools;
struct FocusBoundary{void edit(lv_obj_t* t,const writing::KeyEvent& e){CHECK(e.key==writing::Key::Backspace);lv_textarea_delete_char(t);}}Tab5Focus;
static bool imeTouch(const char*){CHECK(Tab5NumericKeyboard::mode(keyboardTarget)!=0);return false;}
static void rebuildKeyboard(){std::abort();}
static void setKeyboardVisible(bool v){keyboardVisible=v;}
static void keyInsertText(const char* s){lv_textarea_add_text(keyboardTarget,s);}
#include "numeric_event.inc"
static void press(lv_obj_t* parent,const char* token,unsigned mode){
 for(unsigned r=0;r<4;++r)for(const auto& key:Tab5NumericKeyboard::row(mode,r))if(key.value==token){
  auto* b=lv_button_create(parent);lv_obj_add_event_cb(b,keyPressedEvent,LV_EVENT_PRESSED,const_cast<Tab5KeyDef*>(&key));lv_obj_send_event(b,LV_EVENT_PRESSED,nullptr);lv_obj_delete(b);return;
 }
 CHECK(false);
}
int main(){
 lv_init();lv_display_create(1280,720);auto* parent=lv_obj_create(lv_screen_active());auto* input=lv_textarea_create(parent);keyboardTarget=input;lv_textarea_set_one_line(input,true);
 lv_obj_add_event_cb(input,[](lv_event_t*){++ready;},LV_EVENT_READY,nullptr);
 CHECK(Tab5NumericKeyboard::mode(nullptr)==0);CHECK(Tab5NumericKeyboard::mode(parent)==0);CHECK(Tab5NumericKeyboard::mode(input)==0);
 for(auto* rejected:{"abc","0123456789abcdef","123","","0123456789,/"}){lv_textarea_set_accepted_chars(input,rejected);CHECK(Tab5NumericKeyboard::mode(input)==0);}
 for(unsigned cycle=0;cycle<64;++cycle){
  lv_textarea_set_accepted_chars(input,"0123456789.");CHECK(Tab5NumericKeyboard::mode(input)==1);lv_textarea_set_text(input,"");keyboardVisible=true;
  for(auto* key:{"1","0","0",".","5"})press(parent,key,1);CHECK(!strcmp(lv_textarea_get_text(input),"100.5"));
  press(parent,"<BKSP>",1);CHECK(!strcmp(lv_textarea_get_text(input),"100."));press(parent,"7",1);
  press(parent,"<LEFT>",1);CHECK(lv_textarea_get_cursor_pos(input)==4);press(parent,"<RIGHT>",1);CHECK(lv_textarea_get_cursor_pos(input)==5);
  unsigned before=ready;press(parent,"<ENTER>",1);CHECK(ready==before+1);CHECK(!keyboardVisible);
  lv_textarea_set_accepted_chars(input,"-0123456789.");CHECK(Tab5NumericKeyboard::mode(input)==2);lv_textarea_set_text(input,"");
  for(auto* key:{"-","4","5",".","5"})press(parent,key,2);CHECK(!strcmp(lv_textarea_get_text(input),"-45.5"));
  lv_textarea_set_accepted_chars(input,nullptr);CHECK(Tab5NumericKeyboard::mode(input)==0);
 }
 lv_obj_delete(parent);lv_deinit();std::printf("PASS %u numeric keyboard checks; 64 unsigned/signed/return cycles; actual LVGL and production keyPressedEvent\n",checks);
}
