#include <lvgl.h>
#include "Tab5UiStyle.h"
#include "Tab5TextInput.h"
#include "Tab5Focus.h"
#include "Tab5AppPanel.h"
#include "Tab5ContextMenu.h"
#include "Tab5Tools.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
using String=std::string;
static unsigned checks=0;static uint32_t clockMs=0;
static void check(bool c,const char* s){++checks;if(!c){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
uint32_t millis(){return clockMs;}
void tab5DiagnosticMark(unsigned,const char*,const char*,int){}
namespace Tab5AppPanel {static bool running=false;static uint32_t rev=0,snapshots=0;static std::string text;
bool busy(){return running;}uint32_t revision(){return rev;}void clear(){text.clear();++rev;}
static Specification spec;const Specification* specification(){return &spec;}
const char* snapshot(uint32_t& r){++snapshots;r=rev;return text.c_str();}const char* status(){return running?"Analyse en cours.":"Prêt";}}
struct Impl {
 lv_obj_t *toolHeading=nullptr,*root=nullptr,*status=nullptr,*input=nullptr,*output=nullptr,*path=nullptr,*panelMode=nullptr,*panelValueLabel=nullptr,*nativeRun=nullptr,*nativeStop=nullptr,*panelSave=nullptr,*activeField=nullptr,*editButtons[6]{};
 uint32_t panelPaint=0,consoleRevision=UINT32_MAX;bool editing=false;
 struct Hooks{const lv_font_t*(*font)()=nullptr;}hooks;
 const char* tr(const char* text){return text;}
 uint32_t color(unsigned n){return n==2?0xffffff:n==4?0x287eaa:0x202428;}
 Tab5Tool tool=Tab5Tool::AppPanel;void act(int action){if(action==353)clearPanel();}void hideKeyboard(){editing=false;}void inputKeyboard(lv_obj_t* o,bool=false){activeField=o;}
 void note(const char* t){lv_label_set_text(status,t);}
 /* LABEL */
 /* BUTTON */
 /* FIELD */
 /* DROPDOWN */
 /* VISIBLE */
 /* ENABLED */
 /* EDIT_STATE */
 /* EDIT_TOOLBAR */
 /* EDIT_ACTION */
 /* MODE_CHANGED */
 /* CLEAR_PANEL */
 bool key(const writing::KeyEvent& e){auto* a=this;using K=writing::Key;
 /* KEY_PANEL */
 return false;}
 void open(){root=lv_obj_create(lv_screen_active());lv_obj_set_size(root,1160,650);lv_obj_set_style_pad_all(root,16,0);lv_obj_set_style_text_font(root,&lv_font_montserrat_20,0);status=label("Prêt",0,568,1120);
 toolHeading=label("Tool",0,0,950);
 /* OPEN_PANEL */
 Tab5UiStyle::tree(root);lv_obj_update_layout(root);}
 void service(){auto* a=this;
 /* SERVICE_PANEL */
 }
 void close(){Tab5Focus.clear();lv_obj_delete(root);root=nullptr;}
};
static lv_obj_t* findClear(lv_obj_t* object){if(!object)return nullptr;if(lv_obj_check_type(object,&lv_button_class)){for(unsigned i=0;i<lv_obj_get_child_count(object);++i){auto* c=lv_obj_get_child(object,i);if(lv_obj_check_type(c,&lv_label_class)&&!strcmp(lv_label_get_text(c),"Effacer la sortie"))return object;}}for(unsigned i=0;i<lv_obj_get_child_count(object);++i)if(auto* c=findClear(lv_obj_get_child(object,i)))return c;return nullptr;}
int main(){
 auto& spec=Tab5AppPanel::spec;strcpy(spec.title,"Pentest");strcpy(spec.modes,"Vue d'ensemble\nDNS\nRéponse réseau\nPorts TCP\nHTTP et TLS");strcpy(spec.targetLabel,"Cible");strcpy(spec.targetHint,"Adresse IP ou nom d'hôte");strcpy(spec.valueLabel,"Ports TCP");strcpy(spec.value,"22,80,443");strcpy(spec.runLabel,"Analyser");strcpy(spec.alternateHint,"https://exemple.org");spec.valueModes=9;spec.alternateModes=16;spec.modeCount=5;

 lv_init();auto* d=lv_display_create(1280,720);check(d!=nullptr,"LVGL display created");lv_display_set_default(d);
 uint32_t palette[]={0x101418,0x202428,0xffffff,0x777777,0x287eaa,0xffffff,0x172533,0x374451,0x121d26};Tab5UiStyle::configure(palette,8);
 for(unsigned cycle=0;cycle<40;++cycle){
  Impl ui;ui.open();ui.service();check(ui.root&&lv_obj_is_valid(ui.root),"Pentest opens and first service succeeds");
  check(!std::strcmp(lv_textarea_get_text(ui.output),""),"empty initial report is valid text");
  for(unsigned mode=0;mode<5;++mode){lv_dropdown_set_selected(ui.panelMode,mode);lv_obj_send_event(ui.panelMode,LV_EVENT_VALUE_CHANGED,nullptr);check(lv_obj_has_flag(ui.path,LV_OBJ_FLAG_HIDDEN)==(mode!=0&&mode!=3),"port editor follows selected mode");}
  Tab5AppPanel::text="reponse\nligne\n";Tab5AppPanel::rev++;Tab5AppPanel::running=true;clockMs+=600;ui.service();check(std::strstr(lv_textarea_get_text(ui.output),"reponse"),"live report rendered");
  auto* label=lv_textarea_get_label(ui.output);lv_label_set_text_selection_start(label,0);lv_label_set_text_selection_end(label,3);Tab5AppPanel::text+="suivante\n";Tab5AppPanel::rev++;clockMs+=600;ui.service();check(!std::strstr(lv_textarea_get_text(ui.output),"suivante"),"selection is not overwritten");
  lv_textarea_clear_selection(ui.output);Tab5AppPanel::running=false;ui.service();check(std::strstr(lv_textarea_get_text(ui.output),"suivante"),"final result immediate");
  check(lv_obj_has_flag(ui.nativeStop,LV_OBJ_FLAG_HIDDEN),"stop hidden after completion");check(!lv_obj_has_state(ui.panelSave,LV_STATE_DISABLED),"save available for complete report");ui.close();Tab5AppPanel::text.clear();Tab5AppPanel::rev++;
 }
 for(unsigned cycle=0;cycle<4;++cycle){
  Impl ui;ui.open();ui.service();
  std::string report;report.reserve(160*1024);while(report.size()<160*1024-256)report+="443/tcp open service disponible\n";
  Tab5AppPanel::text=report;Tab5AppPanel::rev++;Tab5AppPanel::running=true;clockMs+=500;ui.service();
  check(std::strlen(lv_textarea_get_text(ui.output))==report.size(),"near-160KiB report is rendered completely");
  lv_obj_update_layout(ui.root);lv_obj_scroll_to_y(ui.output,100,LV_ANIM_OFF);const int scroll=lv_obj_get_scroll_y(ui.output);
  const unsigned before=Tab5AppPanel::snapshots;Tab5AppPanel::text+="nouveau resultat\n";Tab5AppPanel::rev++;
  for(unsigned t=0;t<499;++t){++clockMs;ui.service();}
  check(Tab5AppPanel::snapshots==before,"busy report never refreshes before 500ms");++clockMs;ui.service();
  check(Tab5AppPanel::snapshots==before+1,"busy report refreshes at 500ms");
  check(lv_obj_get_scroll_y(ui.output)==scroll,"large report preserves reading position");
  auto* text=lv_textarea_get_label(ui.output);lv_label_set_text_selection_start(text,0);lv_label_set_text_selection_end(text,5);
  Tab5AppPanel::text+="dernier resultat\n";Tab5AppPanel::rev++;clockMs+=500;ui.service();
  check(!std::strstr(lv_textarea_get_text(ui.output),"dernier resultat"),"large report selection remains stable");
  lv_textarea_clear_selection(ui.output);Tab5AppPanel::running=false;ui.service();
  check(std::strlen(lv_textarea_get_text(ui.output))==Tab5AppPanel::text.size(),"large final report refreshes on completion");
  ui.close();Tab5AppPanel::text.clear();Tab5AppPanel::rev++;
 }
 for(unsigned cycle=0;cycle<20;++cycle){
  Impl ui;ui.open();Tab5AppPanel::text="obsolete\n";Tab5AppPanel::rev++;ui.service();
  writing::KeyEvent event;event.key=writing::Key::Text;event.ctrl=true;event.text[0]='l';
  check(ui.key(event),"Ctrl+L is handled");check(!*lv_textarea_get_text(ui.output)&&Tab5AppPanel::text.empty(),"Ctrl+L clears output AND model");
  Tab5AppPanel::text="second\n";Tab5AppPanel::rev++;ui.service();check(Tab5ContextMenu::text(ui.output,100,100),"console context menu opens");
  lv_obj_t* clear=findClear(Tab5ContextMenu::scope());check(clear!=nullptr,"clear offered in context menu");lv_obj_send_event(clear,LV_EVENT_CLICKED,nullptr);Tab5ContextMenu::service();
  check(!*lv_textarea_get_text(ui.output)&&Tab5AppPanel::text.empty(),"context menu clears output AND model");ui.service();check(!*lv_textarea_get_text(ui.output),"later refresh cannot resurrect output");ui.close();
 }
 std::printf("PASS %u real-LVGL ELF panel opening/modes/report/selection/RAZ/context-menu/close checks\n",checks);return 0;
}
