#include "Tab5UsbStorage.h"
#include "Tab5VolumeFS.h"
#include "Tab5FilePicker.h"
#include "Tab5DirectoryView.h"
#include "Tab5FilePolicy.h"
#include "Tab5Localization.h"
#include "Tab5Focus.h"
#include "Tab5Storage.h"
#include "Tab5UiStyle.h"
#include <SD.h>
#include <esp_heap_caps.h>
#include <memory>
#include <new>
namespace Tab5FilePicker {
namespace {
struct State {
 Tab5StorageUse storage{true};
 lv_obj_t *root=nullptr,*heading=nullptr,*up=nullptr,*media=nullptr,*mediaPopup=nullptr,*previous=nullptr;
 std::unique_ptr<Tab5DirectoryView> view;String path,suffix;bool folders=false;unsigned mediaState=0;uint32_t palette=0;
 Result callback=nullptr;void* context=nullptr;
};
State* state=nullptr;
const char* tr(const char* s){return Tab5Locale.text(s);}
lv_color_t color(unsigned index){return lv_color_hex(Tab5UiStyle::color(index));}
uint32_t stamp(){uint32_t hash=Tab5UiStyle::radius();for(unsigned i=0;i<9;++i)hash=hash*33+Tab5UiStyle::color(i);return hash;}
void appearance(lv_obj_t* object){
 if(!object)return;
 if(lv_obj_check_type(object,&lv_button_class)){lv_obj_set_style_bg_color(object,color(1),0);lv_obj_set_style_bg_color(object,color(8),LV_STATE_PRESSED);lv_obj_set_style_border_color(object,color(3),0);lv_obj_set_style_border_width(object,1,0);lv_obj_set_style_shadow_width(object,0,0);Tab5UiStyle::control(object);}
 else if(!lv_obj_check_type(object,&lv_label_class)){lv_obj_set_style_bg_color(object,color(0),0);lv_obj_set_style_border_color(object,color(3),0);}
 Tab5UiStyle::control(object);lv_obj_set_style_text_color(object,color(4),0);for(unsigned i=0;i<lv_obj_get_child_count(object);++i)appearance(lv_obj_get_child(object,i));
}
void scan(const char* path){
 auto* s=state;if(!s)return;s->path=path;lv_label_set_text(s->heading,s->path.c_str());
 if(s->path=="/")lv_obj_add_state(s->up,LV_STATE_DISABLED);else lv_obj_remove_state(s->up,LV_STATE_DISABLED);
 if(!s->view->open(s->path.c_str()))Tab5Focus.focusInitial(s->root,s->media);
}
void parent(){if(!state)return;String path=state->path;int slash=path.lastIndexOf('/');scan(slash<=0?"/":path.substring(0,slash).c_str());}
void select(const char* path){String copy=path;auto fn=state->callback;auto ctx=state->context;close();if(fn)fn(copy.c_str(),ctx);}
lv_obj_t* button(lv_obj_t* parent,const char* text,int x,int y,int w,lv_event_cb_t callback,void* data=nullptr,bool icon=false){
 auto* b=lv_button_create(parent);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,54);
 auto* l=lv_label_create(b);lv_label_set_text(l,icon?text:tr(text));lv_obj_set_width(l,w-20);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_center(l);
 if(icon)lv_obj_set_style_text_font(l,&lv_font_montserrat_24,0);lv_obj_add_event_cb(b,callback,LV_EVENT_CLICKED,data);appearance(b);return b;
}
void closeMedia(){if(!state||!state->mediaPopup)return;Tab5Focus.clear();lv_obj_delete(state->mediaPopup);state->mediaPopup=nullptr;Tab5Focus.focusObject(state->media);}
void openMedia(){
 auto* s=state;if(!s)return;if(s->mediaPopup){closeMedia();return;}
 s->mediaPopup=lv_obj_create(s->root);lv_obj_remove_style_all(s->mediaPopup);lv_obj_set_size(s->mediaPopup,lv_pct(100),lv_pct(100));lv_obj_remove_flag(s->mediaPopup,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_add_event_cb(s->mediaPopup,[](lv_event_t* e){if(state&&lv_event_get_target(e)==state->mediaPopup)closeMedia();},LV_EVENT_CLICKED,nullptr);
 auto* box=lv_obj_create(s->mediaPopup);lv_obj_set_size(box,360,200);lv_obj_center(box);lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(box,14,0);lv_obj_set_style_bg_opa(box,LV_OPA_COVER,0);appearance(box);
 auto* closeButton=button(box,LV_SYMBOL_CLOSE,270,0,54,[](lv_event_t*){closeMedia();},nullptr,true);Tab5Focus.describe(closeButton,tr("Fermer"));lv_obj_t* first=nullptr;int y=0;
 auto option=[&](const char* caption,const char* path){auto* b=button(box,caption,0,y,246,[](lv_event_t* e){String path=static_cast<const char*>(lv_event_get_user_data(e));closeMedia();scan(path.c_str());},const_cast<char*>(path));y+=64;if(!first)first=b;};
 if(tab5SdAvailable())option("Carte SD","/");if(Tab5UsbStorage::mounted())option("USB","/USB");Tab5Focus.focusInitial(s->mediaPopup,first?first:closeButton);
}
}
bool open(const char* directory,const char* suffix,bool folders,Result callback,void* context){
 auto* previous=Tab5Focus.focused();close();void* memory=heap_caps_malloc(sizeof(State),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!memory)return false;
 state=new(memory)State;auto* s=state;if(!s->storage){close();return false;}s->previous=previous;
 s->callback=callback;s->context=context;s->folders=folders;s->suffix=suffix?suffix:"";
 s->root=lv_obj_create(lv_layer_top());lv_obj_set_size(s->root,lv_pct(94),lv_pct(92));lv_obj_center(s->root);lv_obj_set_style_pad_all(s->root,14,0);lv_obj_remove_flag(s->root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_opa(s->root,LV_OPA_COVER,0);
 lv_obj_set_style_text_font(s->root,lv_obj_get_style_text_font(lv_screen_active(),LV_PART_MAIN),0);appearance(s->root);lv_obj_update_layout(s->root);
 int w=lv_obj_get_content_width(s->root),h=lv_obj_get_content_height(s->root);
 s->heading=lv_label_create(s->root);lv_obj_set_pos(s->heading,0,6);lv_obj_set_width(s->heading,w-80);lv_label_set_long_mode(s->heading,LV_LABEL_LONG_DOT);
 auto* closeButton=button(s->root,LV_SYMBOL_CLOSE,w-60,0,60,[](lv_event_t*){close();},nullptr,true);Tab5Focus.describe(closeButton,tr("Fermer"));
 auto* toolbar=lv_obj_create(s->root);lv_obj_remove_style_all(toolbar);lv_obj_set_pos(toolbar,0,62);lv_obj_set_size(toolbar,w,54);lv_obj_set_flex_flow(toolbar,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(toolbar,8,0);lv_obj_remove_flag(toolbar,LV_OBJ_FLAG_SCROLLABLE);
 s->up=button(toolbar,LV_SYMBOL_UP,0,0,54,[](lv_event_t*){parent();},nullptr,true);Tab5Focus.describe(s->up,tr("Dossier parent · Retour arrière"));
 s->media=button(toolbar,LV_SYMBOL_DRIVE,0,0,54,[](lv_event_t*){openMedia();},nullptr,true);Tab5Focus.describe(s->media,tr("Stockage"));
 if(folders){auto* choose=button(toolbar,"Choisir ce dossier",0,0,250,[](lv_event_t*){select(state->path.c_str());});lv_obj_set_flex_grow(choose,1);}
 auto* list=lv_obj_create(s->root);lv_obj_set_pos(list,0,130);lv_obj_set_size(list,w,h-130);
 s->view.reset(new(std::nothrow)Tab5DirectoryView(list,lv_obj_get_style_text_font(s->root,LV_PART_MAIN),[](const char* name,bool dir,void* context){auto* s=static_cast<State*>(context);if(dir)return true;if(s->folders)return false;const size_t n=strlen(name),suffix=s->suffix.length();return !suffix||(n>=suffix&&!strcasecmp(name+n-suffix,s->suffix.c_str()));},[](const char* path,bool dir,void*){if(dir)scan(path);else select(path);},s));
 if(!s->view){close();return false;}
 auto* sort=s->view->sortControl(toolbar,280);if(folders)lv_obj_set_flex_grow(sort,1);
 std::string normalized;const char* start=directory&&Tab5Files::canonical(directory,normalized)?directory:"/";File f=tab5FS.open(start);String path=start;
 if(f&&!f.isDirectory()){int slash=path.lastIndexOf('/');path=slash<=0?"/":path.substring(0,slash);}else if(!f)path=tab5SdAvailable()?"/":Tab5UsbStorage::mounted()?"/USB":"/";f.close();
 s->mediaState=(tab5SdAvailable()?1:0)|(Tab5UsbStorage::mounted()?2:0);s->palette=stamp();scan(path.c_str());return true;
}
void close(){if(!state)return;auto* s=state;state=nullptr;auto* previous=s->previous;Tab5Focus.clear();s->view.reset();if(s->root)lv_obj_delete(s->root);s->~State();heap_caps_free(s);if(previous&&lv_obj_is_valid(previous))Tab5Focus.focusObject(previous);}
bool visible(){return state!=nullptr;}
lv_obj_t* scope(){return state?(state->mediaPopup?state->mediaPopup:state->root):nullptr;}
void service(){
 auto* s=state;if(!s)return;
 if(s->palette!=stamp()){s->palette=stamp();appearance(s->root);s->view->appearance(lv_obj_get_style_text_font(lv_screen_active(),LV_PART_MAIN));}
 const unsigned media=(tab5SdAvailable()?1:0)|(Tab5UsbStorage::mounted()?2:0);
 if(media!=s->mediaState){s->mediaState=media;closeMedia();const bool usb=s->path=="/USB"||s->path.startsWith("/USB/");if((usb&&!(media&2))||(!usb&&!(media&1)))scan(media&1?"/":media&2?"/USB":s->path.c_str());}
 if(media)lv_obj_remove_state(s->media,LV_STATE_DISABLED);else lv_obj_add_state(s->media,LV_STATE_DISABLED);
 if(!s->mediaPopup)s->view->service();
}
bool key(const writing::KeyEvent& e){
 if(!state)return false;
 if(state->mediaPopup){if(e.key==writing::Key::Escape){closeMedia();return true;}return Tab5Focus.key(e,state->mediaPopup);}
 if(e.key==writing::Key::Escape){close();return true;}
 if((e.key==writing::Key::Backspace&&!e.ctrl&&!e.meta)||(e.alt&&e.key==writing::Key::Left)){if(state->path!="/")parent();return true;}
 if(e.key==writing::Key::F5){scan(state->path.c_str());return true;}
 if(state->view->key(e))return true;return Tab5Focus.key(e,state->root);
}
}
