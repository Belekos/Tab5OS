#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Tab5TextInput.cpp"
#include "Tab5UiStyle.cpp"
#include "Tab5ThemeSchema.h"
#include "Tab5ThemeChoices.h"
#include "Tab5TopBarLayout.h"
#include "Tab5DocumentText.h"
#define TAB5_THEME_TEST
#include "Tab5ThemePreview.cpp"
static unsigned checks=0;
static void check(bool value,const char* label){if(!value){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}++checks;}
static void key(lv_obj_t* o,char c,bool shift=false){writing::KeyEvent e;e.key=writing::Key::Text;e.ctrl=true;e.shift=shift;e.text[0]=c;Tab5TextInput::key(o,e);}
static void type(lv_obj_t* o,const char* text){writing::KeyEvent e;e.key=writing::Key::Text;std::snprintf(e.text,sizeof(e.text),"%s",text);Tab5TextInput::key(o,e);}
#include "theme_controls.h"
#include "document_text.h"
int main(int argc,char** argv){
 lv_init();lv_display_create(1280,720);auto* screen=lv_screen_active();Tab5Focus.begin(screen,LV_FONT_DEFAULT);
 auto* input=lv_textarea_create(screen);lv_textarea_set_text(input,"");Tab5TextInput::track(input);
 type(input,"你");type(input,"好");check(!std::strcmp(lv_textarea_get_text(input),"你好"),"UTF-8 typing");key(input,'z');check(!*lv_textarea_get_text(input),"grouped UTF-8 undo");key(input,'y');check(!std::strcmp(lv_textarea_get_text(input),"你好"),"UTF-8 redo");
 lv_tick_inc(700);key(input,'a');Tab5TextInput::insert(input,"日本語");check(!std::strcmp(lv_textarea_get_text(input),"日本語"),"selection replacement");key(input,'z');check(!std::strcmp(lv_textarea_get_text(input),"你好"),"replacement undoes as one transaction");key(input,'z',true);check(!std::strcmp(lv_textarea_get_text(input),"日本語"),"Ctrl Shift Z redo");
 key(input,'z');lv_tick_inc(700);type(input,"!");check(!Tab5TextInput::canRedo(input),"new edit drops redo branch");
 auto* output=lv_textarea_create(screen);lv_textarea_set_text(output,"rapport 漢字");Tab5TextInput::track(output);Tab5TextInput::setReadOnly(output);key(output,'a');key(output,'c');check(!std::strcmp(Tab5TextInput::clipboard(),"rapport 漢字"),"read-only UTF-8 copy");key(output,'x');key(output,'v');key(output,'z');check(!std::strcmp(lv_textarea_get_text(output),"rapport 漢字"),"read-only cannot mutate");check(!Tab5TextInput::canUndo(output),"read-only has no history");
 auto* secret=lv_textarea_create(screen);lv_textarea_set_password_mode(secret,true);lv_textarea_set_text(secret,"");type(secret,"secret");key(secret,'z');check(!std::strcmp(lv_textarea_get_text(secret),"secret"),"password undo unavailable");check(!Tab5TextInput::canUndo(secret),"password excluded from history");
 uint32_t colors[]={0x101010,0x202020,0x303030,0x404040,0xf0f0f0,0xa0a0a0,0x4488aa,0xff0000,0x557799};Tab5UiStyle::configure(colors,0);
 auto* card=lv_obj_create(screen);lv_obj_set_size(card,400,200);lv_obj_set_style_radius(card,12,0);Tab5UiStyle::control(card);check(lv_obj_get_style_radius(card,LV_PART_MAIN)==0,"square appearance");Tab5UiStyle::configure(colors,24);Tab5UiStyle::tree(screen);check(lv_obj_get_style_radius(card,LV_PART_MAIN)==24,"square to rounded live");
 auto* icon=lv_obj_create(screen);lv_obj_set_size(icon,14,14);lv_obj_set_style_radius(icon,4,0);Tab5UiStyle::control(icon);check(lv_obj_get_style_radius(icon,LV_PART_MAIN)==4,"small icon geometry preserved");
 auto* tab=lv_button_create(screen);lv_obj_set_style_radius(tab,0,0);lv_obj_set_style_border_side(tab,LV_BORDER_SIDE_BOTTOM,LV_STATE_CHECKED);Tab5UiStyle::control(tab);check(lv_obj_get_style_radius(tab,LV_PART_MAIN)==0,"tab underline geometry preserved");
 auto* slider=lv_slider_create(screen);Tab5UiStyle::control(slider);check(lv_color_to_u32(lv_obj_get_style_bg_color(slider,LV_PART_INDICATOR))==lv_color_to_u32(lv_color_hex(colors[6])),"slider uses theme accent");
 auto* toggle=lv_switch_create(screen);lv_obj_add_state(toggle,LV_STATE_CHECKED);Tab5UiStyle::control(toggle);check(lv_color_to_u32(lv_obj_get_style_bg_color(toggle,LV_PART_INDICATOR))==lv_color_to_u32(lv_color_hex(colors[6])),"switch checked theme");
 auto* drop=lv_dropdown_create(screen);lv_dropdown_set_options(drop,"A\nB");lv_obj_set_style_text_font(drop,LV_FONT_DEFAULT,0);Tab5UiStyle::control(drop);lv_dropdown_open(drop);check(lv_obj_get_style_text_font(lv_dropdown_get_list(drop),LV_PART_MAIN)==LV_FONT_DEFAULT,"dropdown popup inherits font");check(lv_color_to_u32(lv_obj_get_style_bg_color(lv_dropdown_get_list(drop),LV_PART_MAIN))==lv_color_to_u32(lv_color_hex(colors[1])),"dropdown popup inherits theme");lv_dropdown_close(drop);
 lv_obj_delete(input);lv_obj_delete(output);lv_obj_delete(secret);check(Tab5TextInput::historyBytes==0,"deleted fields release all history");
 for(unsigned n=0;n<100;++n){auto* o=lv_textarea_create(screen);lv_textarea_set_text(o,"");Tab5TextInput::track(o);for(unsigned j=0;j<60;++j){lv_tick_inc(700);type(o,"é");}check(Tab5TextInput::historyBytes<=131072,"bounded editing memory");lv_obj_delete(o);}
 check(Tab5TextInput::historyBytes==0,"repeated field deletion releases history");
 themeChecks(screen);
 check(argc==2,"shipped document font supplied");documentTextChecks(screen,argv[1]);
 std::printf("PASS %u checks; actual LVGL %d.%d.%d native widgets; no hardware test\n",checks,LVGL_VERSION_MAJOR,LVGL_VERSION_MINOR,LVGL_VERSION_PATCH);return 0;
}
