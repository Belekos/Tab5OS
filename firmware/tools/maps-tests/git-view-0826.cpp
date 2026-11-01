#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <new>
#include <string>
#include <vector>
#include <lvgl.h>
#include "Tab5GitView.h"
#include "Tab5ContextMenu.h"
#include "Tab5Git.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5UiStyle.h"
#include "Tab5TerminalBuffer.h"
static unsigned checks=0;static bool inCloseEvent=false;static unsigned deletedInsideEvent=0;
static void deletionProbe(lv_event_t*){if(inCloseEvent)++deletedInsideEvent;}
static void check(bool ok,const char* why){++checks;
#ifdef TAB5_TEST_BASELINE
 fprintf(stderr,"CHECK %u %s = %d\n",checks,why,int(ok));fflush(stderr);
#endif
 if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static uint32_t millis(){return 100000;}
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,portMAX_DELAY=0,pdPASS=1;
using StaticSemaphore_t=int;using SemaphoreHandle_t=int*;
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* p){return p;}
void xSemaphoreTake(SemaphoreHandle_t,int){}void xSemaphoreGive(SemaphoreHandle_t){}
static std::map<void*,size_t> allocations;
void* heap_caps_malloc(size_t n,int){void* p=malloc(n);if(p)allocations[p]=n;return p;}
void heap_caps_free(void* p){if(p){check(allocations.erase(p)==1,"free owned model once");free(p);}}
struct Locale{size_t copyText(const char* text,char* out,size_t cap){if(out&&cap){strncpy(out,text,cap-1);out[cap-1]=0;}return strlen(text);}}Tab5Locale;
namespace Tab5Resources {enum class Owner{Tools};struct Request{size_t a,b,c,d,e;};struct Reservation {explicit operator bool()const{return true;}void allocated(size_t,size_t){}};}
Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Request,const char*){return {};}
void(*pendingTask)(void*)=nullptr;void* pendingArg=nullptr;
void tab5TaskCheckpoint(const char*){}
template<class F,class H>int tab5CreateTask(F fn,const char*,uint32_t,void* arg,unsigned,H,int){pendingTask=fn;pendingArg=arg;return pdPASS;}
static bool mockRepository=true,mockStorageFailure=false,mockFetchFailure=false,mockUnborn=false,mockDetached=false;static unsigned mockCount=300;
static std::vector<std::vector<std::string>> commands;
static std::vector<std::string> directories;
static std::string mockDiscovery;
namespace Tab5Git {
bool busy(){return false;}
int run(const std::string&,const std::string& cwd,const std::vector<std::string>& args,Output out,Cancel cancel,Translate,Output,Observer observer){
 commands.push_back(args);directories.push_back(cwd);if(cancel&&cancel())return 130;
 if(args[1]=="init"||args[1]=="clone")mockRepository=true;
 if(args[1]=="fetch"&&mockFetchFailure){if(out)out("Network error\n",14);return 1;}
 if(!mockRepository){if(!mockStorageFailure&&observer.notRepository)observer.notRepository();return 1;}
 if(observer.project)observer.project(mockDiscovery.empty()?cwd.c_str():mockDiscovery.c_str());
 if(observer.repository)observer.repository("main",mockUnborn,mockDetached);
 if(observer.remote)observer.remote("origin","https://example.org/project.git");
 if(observer.config){observer.config("user.name","Utilisateur");observer.config("user.email","user@example.invalid");}
 for(unsigned i=0;i<mockCount;++i){char path[64];snprintf(path,sizeof(path),"src/file-%04u.txt",i);if(observer.status)observer.status(path,WorkModified);}
 if(out)out("Git result\n",11);return 0;
}
MemoryStats memoryStats(){return {};}
}
namespace Tab5Secrets {std::vector<std::string> names(){return {"GIT_USER","GIT_TOKEN"};}}
#include "focus-os.inc"
/* PRODUCTION */
static void finish(){check(pendingTask!=nullptr,"worker queued");auto fn=pendingTask;auto arg=pendingArg;pendingTask=nullptr;fn(arg);check(!Tab5GitView::busy(),"worker publishes its last model access before returning");Tab5GitView::service();lv_obj_update_layout(lv_screen_active());}
static writing::KeyEvent key(writing::Key k,bool alt=false){writing::KeyEvent e{};e.key=k;e.alt=alt;return e;}
static lv_display_t* display;
static lv_font_t* textFont;
static std::vector<unsigned char> pixels(1280*720*3),drawBuffer(1280*720*3);
static void flush(lv_display_t* d,const lv_area_t* area,uint8_t* bytes){
 for(int y=area->y1;y<=area->y2;++y){const size_t n=size_t(area->x2-area->x1+1)*3;memcpy(pixels.data()+(y*1280+area->x1)*3,bytes,n);bytes+=n;}lv_display_flush_ready(d);
}
static void capture(const char* name,unsigned height=720){
 lv_obj_update_layout(lv_screen_active());lv_refr_now(display);std::ofstream out(name,std::ios::binary);out<<"P6\n1280 "<<height<<"\n255\n";
 for(unsigned i=0;i<1280*height*3;i+=3){out.put(pixels[i+2]);out.put(pixels[i+1]);out.put(pixels[i]);}
}
int main(int argc,char** argv){
 lv_init();check(argc==2,"shipped font argument");std::ifstream fontFile(argv[1],std::ios::binary);std::vector<unsigned char> fontBytes((std::istreambuf_iterator<char>(fontFile)),std::istreambuf_iterator<char>());
 textFont=lv_tiny_ttf_create_data(fontBytes.data(),fontBytes.size(),20);check(textFont!=nullptr,"shipped font loaded");textFont->fallback=&lv_font_montserrat_20;
 display=lv_display_create(1280,720);check(display!=nullptr,"display");
 lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,drawBuffer.data(),nullptr,drawBuffer.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,flush);
 lv_display_set_theme(display,lv_theme_mono_init(display,true,textFont));Tab5Focus.begin(lv_screen_active(),textFont);
 for(unsigned i=0;i<25;++i){
  auto* parent=lv_obj_create(lv_screen_active());auto* button=lv_button_create(parent);
  lv_obj_add_event_cb(button,[](lv_event_t* e){lv_obj_delete(static_cast<lv_obj_t*>(lv_event_get_user_data(e)));},LV_EVENT_CLICKED,parent);
  check(lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr)==LV_RESULT_INVALID,"LVGL control: ordinary parent deletion during event is supported");
  check(!lv_obj_is_valid(parent),"LVGL control: parent actually removed");
 }
 uint32_t palette[]={0x191919,0x232323,0x333333,0x555555,0xeeeeee,0xaaaaaa,0x5577aa,0xffffff,0x3b485b};Tab5UiStyle::configure(palette,8);
 Tab5GitView::Hooks hooks;hooks.font=[]()->const lv_font_t*{return textFont;};hooks.keyboard=[](bool visible,lv_obj_t* field){setKeyboardVisible(visible,field);};
 keyboardPanel=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(keyboardPanel);lv_obj_set_size(keyboardPanel,0,0);lv_obj_add_flag(keyboardPanel,LV_OBJ_FLAG_HIDDEN);
 /* FOCUS OBSERVER */
 check(Tab5GitView::open(lv_screen_active(),"/Projects/demo",hooks),"open repository");
 check(allocations.size()==1&&Tab5GitView::busy(),"single bounded PSRAM model");finish();
 using namespace Tab5GitView;
 capture("git-view-dark.ppm");
 check(model->count==128&&model->total==300,"exact count, first bounded page");
 const std::string idleState=lv_label_get_text(state);action(Stop);check(idleState==lv_label_get_text(state),"cancel at rest has no effect");
 mockFetchFailure=true;action(Fetch);finish();mockFetchFailure=false;
 check(model->result==1&&model->repository&&model->remote[0]&&model->count==128,"failed transfer preserves diagnostic and refreshes local actions");
 check(!lv_obj_has_flag(buttons[Fetch],LV_OBJ_FLAG_HIDDEN),"failed network operation remains retryable");
 mockUnborn=true;action(Status);finish();check(lv_obj_has_flag(buttons[Push],LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(buttons[Pull],LV_OBJ_FLAG_HIDDEN)&&!lv_obj_has_flag(buttons[Fetch],LV_OBJ_FLAG_HIDDEN),"unborn repository only exposes meaningful remote operation");
 mockUnborn=false;mockDetached=true;action(Status);finish();check(lv_obj_has_flag(buttons[Push],LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(buttons[Pull],LV_OBJ_FLAG_HIDDEN),"detached repository cannot request branch-only operations");mockDetached=false;action(Status);finish();
 check(lv_obj_get_child_count(list)==VisibleRows+1,"only recycled LVGL rows");
 check(!lv_obj_has_flag(buttons[Next],LV_OBJ_FLAG_HIDDEN),"next page contextual");
 focusEntry(0);
 for(unsigned i=0;i<127;++i)Tab5GitView::key(key(writing::Key::Down));
 bool found=false;for(unsigned i=0;i<VisibleRows;++i)if(Tab5Focus.focused()==row[i])found=rowIndex[i]==127;
 check(found,"keyboard reaches last row through recycling");
 Tab5GitView::key(key(writing::Key::Right,true));finish();
 check(model->offset==128&&model->count==128,"second page accessible");
 Tab5GitView::key(key(writing::Key::Right,true));finish();
 check(model->offset==256&&model->count==44,"last page accessible");
 check(lv_obj_has_flag(buttons[Next],LV_OBJ_FLAG_HIDDEN),"no meaningless next action");
 selected=10;const std::string expected=model->entries[10].path;action(Stage);finish();
 check(commands[commands.size()-2]==std::vector<std::string>({"git","add","--",expected}),"stage selected path without shell parsing");
 check(model->offset==256&&selected==10,"stage preserves page and selected file");
 action(Status);check(lv_obj_has_state(list,LV_STATE_DISABLED),"list disabled while worker owns model");
 const int previousScroll=lv_obj_get_scroll_y(list);Tab5GitView::key(key(writing::Key::PageDown));check(lv_obj_get_scroll_y(list)==previousScroll,"keyboard cannot scroll stale rows during worker");
 const int previousSelection=selected;lv_obj_send_event(row[0],LV_EVENT_CLICKED,nullptr);
 check(selected==previousSelection,"queued click cannot select during worker");finish();
 showPrompt(Diff);lv_dropdown_open(choice);
 check(lv_color_to_u32(lv_obj_get_style_bg_color(lv_dropdown_get_list(choice),LV_PART_MAIN))==lv_color_to_u32(lv_color_hex(palette[1])),"Diff options use OS surface color");
 check(lv_obj_get_style_text_font(lv_dropdown_get_list(choice),LV_PART_MAIN)==textFont,"Diff options use delivered UI font");
 lv_dropdown_close(choice);lv_dropdown_set_selected(choice,1);submitPrompt();finish();
 check(commands[commands.size()-2]==std::vector<std::string>({"git","diff","--cached"}),"staged differences accessible");
 showPrompt(Clone);lv_obj_update_layout(lv_screen_active());
 check(Tab5Focus.focused()==fields[0],"clone opens at required URL");

 // Reproduce the user's exact sequence: Tab moves to the second field,
 // then a physical/tactile click selects the first field again.
 Tab5GitView::key(key(writing::Key::Tab));check(Tab5Focus.focused()==fields[1],"Tab selects second clone field");
 lv_obj_send_event(fields[0],LV_EVENT_CLICKED,nullptr);
 check(Tab5Focus.focused()==fields[0]&&keyboardTarget==fields[0],"touch supersedes last Tab focus globally");
 Tab5Focus.finishEditing();lv_obj_send_event(fields[0],LV_EVENT_CLICKED,nullptr);check(Tab5Focus.controlling(),"touch re-enters same field after leaving edit mode");
 writing::KeyEvent typed{};typed.key=writing::Key::Text;strcpy(typed.text,"x");Tab5GitView::key(typed);
 check(std::string(lv_textarea_get_text(fields[0]))=="x"&&std::string(lv_textarea_get_text(fields[1])).empty(),"physical typing follows touched input");
 Tab5GitView::key(key(writing::Key::Tab));check(keyboardTarget==fields[1],"Tab changes tactile keyboard target too");
 Tab5TextInput::insert(keyboardTarget,"y");check(std::string(lv_textarea_get_text(fields[1]))=="y","tactile typing follows Tab input");
 Tab5GitView::key(key(writing::Key::Tab,true)); // Alt is irrelevant to Tab ordering.
 lv_obj_send_event(fields[0],LV_EVENT_CLICKED,nullptr);Tab5Focus.key(key(writing::Key::Tab),scope());
 check(Tab5Focus.controlling()&&Tab5Focus.focused()==fields[1],"generic Tab enters text editing immediately");
 Tab5Focus.key(typed,scope());check(std::string(lv_textarea_get_text(fields[1]))=="yx","generic dialog receives text after Tab without Enter");
 check(lv_obj_get_height(fields[0])>=44&&lv_obj_get_height(fields[1])>=44,"single-line inputs retain touch height after LVGL mode setter");
 lv_textarea_set_text(fields[0],"https://example.org/project.git");lv_textarea_set_text(fields[1],"clone name");
 lv_obj_send_event(fields[1],LV_EVENT_CLICKED,nullptr);lv_obj_update_layout(lv_screen_active());
 lv_area_t a;lv_obj_get_coords(confirm,&a);check(a.y2<432,"confirm remains above tactile keyboard");
 check(lv_obj_has_flag(promptBody,LV_OBJ_FLAG_SCROLLABLE),"modal fields can scroll");
 
#ifndef TAB5_TEST_BASELINE
 check(secretChoices[0]&&secretChoices[1],"native clone exposes both vault selectors");
 lv_dropdown_set_selected(secretChoices[0],1);submitPrompt();check(prompt&&!busy(),"partial vault authentication refused before starting worker");lv_dropdown_set_selected(secretChoices[1],2);
 
#endif
 submitPrompt();finish();check(std::string(model->directory)=="/Projects/demo/clone name","clone enters new project");
 
#ifndef TAB5_TEST_BASELINE
 check(commands[commands.size()-2]==std::vector<std::string>({"git","clone","--username-secret","GIT_USER","--token-secret","GIT_TOKEN","https://example.org/project.git","clone name"}),"native clone queues names only");
 showPrompt(Settings);lv_dropdown_set_selected(choice,4);lv_obj_send_event(choice,LV_EVENT_VALUE_CHANGED,nullptr);check(secretChoices[0]&&lv_obj_has_flag(fields[0],LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(promptEdits,LV_OBJ_FLAG_HIDDEN),"username secret has actual selector without irrelevant text actions");lv_dropdown_set_selected(secretChoices[0],1);submitPrompt();finish();check(commands[commands.size()-2]==std::vector<std::string>({"git","config","tab5.usernameSecret","GIT_USER"}),"username reference stored via native settings");
 showPrompt(Settings);lv_dropdown_set_selected(choice,5);lv_obj_send_event(choice,LV_EVENT_VALUE_CHANGED,nullptr);lv_dropdown_set_selected(secretChoices[0],0);submitPrompt();finish();check(commands[commands.size()-2]==std::vector<std::string>({"git","config","--unset","tab5.tokenSecret"}),"none removes reference explicitly");
 
 
#endif
 showPrompt(Settings);lv_dropdown_open(choice);Tab5GitView::key(key(writing::Key::Escape));
 check(prompt&&!lv_dropdown_is_open(choice),"Escape closes dropdown before dialog");
 Tab5GitView::key(key(writing::Key::Escape));check(!prompt,"second Escape closes dialog");Tab5GitView::service();
 action(Status);const auto* active=model;close();check(!scope()&&model==active&&!allocations.empty(),"closing running job retains worker context");finish();check(!model&&allocations.empty(),"ordinary context freed only after last model-access barrier, not an RTOS destruction claim");
 for(unsigned i=0;i<400;++i){
 check(open(lv_screen_active(),"/Projects/demo",hooks),"repeat open");finish();
 lv_obj_add_event_cb(window,deletionProbe,LV_EVENT_DELETE,nullptr);
 auto* oldWindow=window;auto* exit=lv_obj_get_child(window,1);
 inCloseEvent=true;lv_obj_send_event(exit,LV_EVENT_CLICKED,nullptr);inCloseEvent=false;
 check(!scope()&&lv_obj_is_valid(oldWindow),"event detaches and hides Git immediately");
 check(!deletedInsideEvent,"tree destruction never nested in input callback");
 check(allocations.size()==1,"model retained until ordinary service boundary");
 service();check(!lv_obj_is_valid(oldWindow)&&allocations.empty(),"service frees tree and PSRAM exactly once");
 }
 mockRepository=false;check(open(lv_screen_active(),"/Projects/plain",hooks),"open ordinary directory");finish();
 check(std::string(lv_label_get_text(state))=="Dossier sans dépôt Git"&&Tab5Focus.focused()==buttons[New],"ordinary folder is a normal creation state");close();service();
 mockStorageFailure=true;check(open(lv_screen_active(),"/Projects/error",hooks),"open with pending storage failure");finish();
 check(std::string(lv_label_get_text(state))=="Opération Git en échec","storage failure is not hidden as plain directory");close();service();service();
 mockRepository=true;mockStorageFailure=false;mockDiscovery="/Projects/root";
 check(open(lv_screen_active(),"/Projects/root/src",hooks),"open repository subdirectory");finish();selected=0;action(Stage);finish();
 check(directories[directories.size()-2]==mockDiscovery,"stage uses discovered repository root");close();service();service();
 uint32_t light[]={0xf4f4f0,0xffffff,0xe8e8e4,0x777777,0x161616,0x666666,0x365b78,0x161616,0xcbdce8};Tab5UiStyle::configure(light,8);lv_display_set_theme(display,lv_theme_mono_init(display,false,textFont));
 check(open(lv_screen_active(),"/Projects/demo",hooks),"light theme open");finish();showPrompt(Settings);lv_obj_send_event(fields[0],LV_EVENT_CLICKED,nullptr);lv_obj_update_layout(lv_screen_active());
 lv_obj_get_coords(confirm,&a);check(a.y2<432,"settings confirm above OS keyboard boundary");capture("git-view-settings-light.ppm",432);close();service();service();
#ifndef TAB5_TEST_BASELINE
 check(open(lv_screen_active(),"/Projects/demo",hooks),"clear output view");finish();
 append("before clear",12);service();check(strlen(lv_textarea_get_text(report))>0,"output painted");
 auto clearKey=key(writing::Key::Text);clearKey.ctrl=true;clearKey.text[0]='l';
 Tab5GitView::key(clearKey);check(model->output.size()==0&&!strlen(lv_textarea_get_text(report)),"Ctrl+L clears model and widget");
 append("new",3);service();check(!strcmp(lv_textarea_get_text(report),"new"),"clear cannot restore prior output");
 close();service();mockDiscovery.clear();
 for(auto start:{Start::Create,Start::Import}){check(open(lv_screen_active(),"/Projects",hooks,start),"menu startup");finish();check(prompt&&promptAction==(start==Start::Create?New:Clone),"create/import starts required prompt");close();service();}
 check(open(lv_screen_active(),"/Projects/plain",hooks,Start::Initialize),"initialize selected directory");finish();check(commands[commands.size()-2]==std::vector<std::string>({"git","init","."}),"initialize existing folder without extra nested folder");close();service();
#endif
 lv_deinit();printf("PASS %u Git UI production LVGL/lifecycle checks\n",checks);return 0;
}
