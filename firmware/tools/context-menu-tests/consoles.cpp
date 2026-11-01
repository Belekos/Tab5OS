#include <lvgl.h>
#include "Tab5UiStyle.h"
#include "Tab5TextInput.h"
#include "Tab5Focus.h"
#include "Tab5ContextMenu.h"
#include "Tab5Tools.h"
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
using String=std::string;
static unsigned checks=0;
static uint32_t clockMs=0;
static void check(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
uint32_t millis(){return clockMs;}
struct Model {std::string text;uint32_t revision=0;unsigned clears=0;void clear(){text.clear();++revision;++clears;}void append(const char* value){text+=value;++revision;}};
static Model terminal,python,executable;
namespace Tab5Terminal {void clearOutput(){terminal.clear();}bool busy(){return false;}uint32_t revision(){return terminal.revision;}const char* snapshot(uint32_t& rev){rev=terminal.revision;return terminal.text.c_str();}String directory(){return "/";}}
namespace Tab5Python {void clearOutput(){python.clear();}bool running(){return true;}bool failed(){return false;}uint32_t outputRevision(){return python.revision;}String output(){return python.text;}}
void tab5ExecutableClearOutput(){executable.clear();}bool tab5ExecutableBusy(){return false;}const char* tab5ExecutableStatus(){return "Prêt";}uint32_t tab5ExecutableRevision(){return executable.revision;}const char* tab5ExecutableConsoleSnapshot(){return executable.text.c_str();}
struct Impl {
 lv_obj_t *root=nullptr,*output=nullptr,*consoleClear=nullptr,*nativeStop=nullptr,*nativeRun=nullptr,*status=nullptr,*nativeConsole=nullptr,*switchPrompt=nullptr;
 uint32_t consoleRevision=UINT32_MAX,terminalPaint=0,nativeLast=0;bool pythonWasRunning=false;
 Tab5Tool tool=Tab5Tool::Terminal;
 const char* tr(const char* text){return text;}uint32_t color(unsigned n){return n==2?0xffffff:0x202428;}
 void note(const char* text){lv_label_set_text(status,text);}void scriptUI(){}void close(bool=false){Tab5Focus.clear();lv_obj_delete(root);root=output=nullptr;}
 /* BUTTON */
 /* BIND_CONSOLE */
 /* VISIBLE */
 void act(int action){/* CLEAR_ACTION */}
 bool key(const writing::KeyEvent& e){auto* a=this;using K=writing::Key;/* CLEAR_KEY */return false;}
 void service(){auto* a=this;/* CLEAR_VISIBILITY */
 /* TERMINAL_SERVICE */
 /* EXECUTABLE_SERVICE */
 /* PYTHON_SERVICE */
 }
 void open(Tab5Tool which){tool=which;root=lv_obj_create(lv_screen_active());lv_obj_set_size(root,1200,650);nativeStop=lv_button_create(root);nativeRun=lv_button_create(root);status=lv_label_create(root);nativeConsole=lv_obj_create(root);lv_obj_set_size(nativeConsole,1000,300);output=which==Tab5Tool::Terminal?lv_textarea_create(nativeConsole):lv_label_create(nativeConsole);lv_obj_set_size(output,980,250);if(which==Tab5Tool::Terminal){Tab5TextInput::setReadOnly(output);lv_textarea_set_text(output,"");}else{lv_label_set_text(output,"");Tab5TextInput::selectable(output);}bindConsole(1020,10);lv_obj_update_layout(root);}
 const char* displayed(){return tool==Tab5Tool::Terminal?lv_textarea_get_text(output):lv_label_get_text(output);}
};
static lv_obj_t* findClear(lv_obj_t* at){if(!at)return nullptr;if(lv_obj_check_type(at,&lv_button_class)){for(unsigned i=0;i<lv_obj_get_child_count(at);++i){auto* child=lv_obj_get_child(at,i);if(lv_obj_check_type(child,&lv_label_class)&&!strcmp(lv_label_get_text(child),"Effacer la sortie"))return at;}}for(unsigned i=0;i<lv_obj_get_child_count(at);++i)if(auto* found=findClear(lv_obj_get_child(at,i)))return found;return nullptr;}
int main(){lv_init();auto* display=lv_display_create(1280,720);lv_display_set_default(display);uint32_t palette[]={0x101418,0x202428,0xffffff,0x777777,0x287eaa,0xffffff,0x172533,0x374451,0x121d26};Tab5UiStyle::configure(palette,8);
 for(auto kind:{Tab5Tool::Terminal,Tab5Tool::Python,Tab5Tool::Executable})for(unsigned cycle=0;cycle<12;++cycle){
  Impl ui;ui.open(kind);Model& model=kind==Tab5Tool::Terminal?terminal:kind==Tab5Tool::Python?python:executable;
  check(lv_obj_has_flag(ui.consoleClear,LV_OBJ_FLAG_HIDDEN),"empty output has no clear control");
  for(unsigned route=0;route<3;++route){
   model.append("old output\n");clockMs+=600;ui.service();ui.service();check(strstr(ui.displayed(),"old output")!=nullptr,"production service displays backing model");check(!lv_obj_has_flag(ui.consoleClear,LV_OBJ_FLAG_HIDDEN),"clear control visible when populated");
   auto* label=kind==Tab5Tool::Terminal?lv_textarea_get_label(ui.output):ui.output;lv_label_set_text_selection_start(label,0);lv_label_set_text_selection_end(label,3);unsigned old=model.clears;
   if(route==0)lv_obj_send_event(ui.consoleClear,LV_EVENT_CLICKED,nullptr);
   else if(route==1){writing::KeyEvent event;event.key=writing::Key::Text;event.ctrl=true;event.text[0]='l';check(ui.key(event),"Ctrl+L handled");}
   else {check(Tab5ContextMenu::text(ui.output,100,100),"right click opens console context menu");auto* clear=findClear(Tab5ContextMenu::scope());check(clear!=nullptr,"context offers clear");lv_obj_send_event(clear,LV_EVENT_CLICKED,nullptr);Tab5ContextMenu::service();}
   check(model.clears==old+1&&model.text.empty(),"correct backing model cleared once");check(!*ui.displayed(),"visible output cleared");check(!Tab5TextInput::hasSelection(ui.output),"old selection cleared");clockMs+=600;ui.service();check(!*ui.displayed(),"production refresh does not restore old output");check(lv_obj_has_flag(ui.consoleClear,LV_OBJ_FLAG_HIDDEN),"empty clear control hidden");
   model.append("new output\n");clockMs+=600;ui.service();check(strstr(ui.displayed(),"new output")&&!strstr(ui.displayed(),"old output"),"new output survives clear without stale lines");model.clear();clockMs+=600;ui.service();
  }
  ui.close();check(ui.consoleClear==nullptr,"delete callback clears button pointer");
 }
 std::printf("PASS %u console UI checks; 36 open/close sessions; Terminal/Python/ELF button, Ctrl+L, context, selection and production refresh\n",checks);return 0;}
