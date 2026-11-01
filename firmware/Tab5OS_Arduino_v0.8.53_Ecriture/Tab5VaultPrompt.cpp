#include "Tab5VaultPrompt.h"
#include "Tab5KeyStore.h"
#include "Tab5Vault.h"
#include "Tab5VaultCore.h"
#include "Tab5TextInput.h"
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "WriterUsb.h"
#include <algorithm>
#include <cstring>

namespace Tab5VaultPrompt {
namespace {
Hooks hooks;
lv_obj_t *overlay=nullptr,*pin=nullptr,*confirmation=nullptr,*status=nullptr,*submit=nullptr;
Done completion=nullptr;
void* completionContext=nullptr;
bool pending=false,initialize=false,closing=false;
const char* tr(const char* text){return hooks.translate?hooks.translate(text):text;}
lv_color_t color(unsigned role){return lv_color_hex(hooks.color?hooks.color(role):role==2?0xffffff:0x202428);}
void enabled(lv_obj_t* object,bool yes){if(!object)return;if(yes)lv_obj_remove_state(object,LV_STATE_DISABLED);else lv_obj_add_state(object,LV_STATE_DISABLED);}
void erase(lv_obj_t* field){
 if(!field)return;
 // LVGL owns this mutable password buffer. Erase before releasing the field;
 // neither the clipboard nor an undo history is permitted for protected input.
 char* value=const_cast<char*>(lv_textarea_get_text(field));
 if(value)tab5vault::wipe(value,strlen(value));
 lv_textarea_set_text(field,"");
}
bool validPin(){
 const char* text=pin?lv_textarea_get_text(pin):"";const size_t n=strlen(text);
 if(n<4||n>12)return false;
 for(size_t i=0;i<n;++i)if(text[i]<'0'||text[i]>'9')return false;
 return !confirmation||!strcmp(text,lv_textarea_get_text(confirmation));
}
void refresh(){
 const bool idle=!pending&&!Tab5KeyStore::busy()&&!Tab5Vault::busy();
 enabled(pin,idle);enabled(confirmation,idle);
 enabled(submit,idle&&!Tab5KeyStore::pinRetrySeconds()&&validPin());
}
void finish(bool accepted){
 if(!overlay||closing)return;
 closing=true;
 const auto callback=completion;void* context=completionContext;
 completion=nullptr;completionContext=nullptr;
 if(pending&&!accepted)Tab5KeyStore::lock(); // invalidates an in-flight unlock
 pending=false;Tab5KeyStore::finishUi();
 erase(pin);erase(confirmation);
 if(hooks.keyboard)hooks.keyboard(false,nullptr);
 if(Tab5Focus.contains(overlay,Tab5Focus.focused()))Tab5Focus.clear();
 auto* old=overlay;overlay=pin=confirmation=status=submit=nullptr;
 lv_obj_delete(old);closing=false;
 // Also restore when opened and cancelled between two OS service ticks.
 // Focus remembers deletion-watched objects, never a raw caller widget here.
 Tab5Focus.restore(lv_screen_active());
 if(callback)callback(accepted,context);
}
void accept(){
 if(!overlay||pending||Tab5KeyStore::busy()||Tab5Vault::busy())return;
 if(!validPin()){
  lv_label_set_text(status,tr(confirmation&&strcmp(lv_textarea_get_text(pin),lv_textarea_get_text(confirmation))?"Les PIN diffèrent":"PIN : 4 à 12 chiffres"));return;
 }
 if(Tab5KeyStore::pinRetrySeconds()){lv_label_set_text(status,tr(Tab5KeyStore::status()));return;}
 pending=Tab5KeyStore::beginDeviceUnlock(lv_textarea_get_text(pin),initialize,Tab5KeyStore::unlocked()&&!Tab5KeyStore::usingDevice(),true);
 if(pending){erase(pin);erase(confirmation);if(hooks.keyboard)hooks.keyboard(false,nullptr);Tab5Focus.finishEditing();}
 lv_label_set_text(status,tr(Tab5KeyStore::status()));refresh();
}
void ready(lv_obj_t* field){
 if(field==pin&&confirmation){Tab5Focus.editObject(confirmation);if(hooks.keyboard)hooks.keyboard(!writing::usbConnected(),confirmation);return;}
 accept();
}
lv_obj_t* label(lv_obj_t* box,const char* text,int y,int width){
 auto* object=lv_label_create(box);lv_obj_set_pos(object,0,y);lv_obj_set_width(object,width);
 lv_label_set_long_mode(object,LV_LABEL_LONG_WRAP);lv_label_set_text(object,tr(text));return object;
}
lv_obj_t* field(lv_obj_t* box,int y,int width){
 auto* object=lv_textarea_create(box);lv_obj_set_pos(object,0,y);lv_obj_set_size(object,width,52);
 lv_textarea_set_one_line(object,true);Tab5TextInput::protect(object);
 lv_textarea_set_password_mode(object,true);lv_textarea_set_password_show_time(object,0);
 lv_textarea_set_max_length(object,12);lv_textarea_set_accepted_chars(object,"0123456789");
 lv_obj_set_style_bg_color(object,color(0),0);lv_obj_set_style_text_color(object,color(2),0);Tab5UiStyle::control(object);
 lv_obj_add_event_cb(object,[](lv_event_t*){refresh();},LV_EVENT_VALUE_CHANGED,nullptr);
 lv_obj_add_event_cb(object,[](lv_event_t* e){auto* target=lv_event_get_target_obj(e);Tab5Focus.editObject(target);if(hooks.keyboard)hooks.keyboard(!writing::usbConnected(),target);},LV_EVENT_CLICKED,nullptr);
 lv_obj_add_event_cb(object,[](lv_event_t* e){ready(lv_event_get_target_obj(e));},LV_EVENT_READY,nullptr);
 return object;
}
lv_obj_t* button(lv_obj_t* box,const char* title,int x,int y,int width,bool confirm){
 auto* object=lv_button_create(box);lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,48);
 auto* text=lv_label_create(object);lv_label_set_text(text,tr(title));lv_obj_center(text);Tab5UiStyle::button(object);
 lv_obj_add_event_cb(object,[](lv_event_t* e){if(lv_event_get_user_data(e))accept();else finish(false);},LV_EVENT_CLICKED,confirm?reinterpret_cast<void*>(1):nullptr);
 return object;
}
}
void begin(Hooks value){hooks=value;}
bool open(Done done,void* context){
 if(overlay||Tab5KeyStore::busy()||Tab5Vault::busy())return false;
 if(Tab5KeyStore::credentialsAccessible()){if(done)done(true,context);return true;}
 completion=done;completionContext=context;pending=false;initialize=!Tab5KeyStore::pinConfigured();
 Tab5Focus.remember(lv_screen_active());
 // The OS touch keyboard is a sibling on the active screen. Using layer_top
 // here would cover its numeric keys even after keyboard(true) raises them.
 overlay=lv_obj_create(lv_screen_active());
 if(!overlay){completion=nullptr;completionContext=nullptr;return false;}
 lv_obj_set_size(overlay,lv_pct(100),lv_pct(100));lv_obj_set_pos(overlay,0,0);
 lv_obj_set_style_bg_color(overlay,lv_color_black(),0);lv_obj_set_style_bg_opa(overlay,LV_OPA_50,0);
 lv_obj_set_style_border_width(overlay,0,0);lv_obj_set_style_pad_all(overlay,0,0);lv_obj_remove_flag(overlay,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_add_event_cb(overlay,[](lv_event_t* e){
  if(lv_event_get_target_obj(e)!=overlay)return;
  // External teardown must also cancel the continuation and forget every
  // field before LVGL recursively deletes the children.
  if(pending)Tab5KeyStore::lock();pending=false;Tab5KeyStore::finishUi();
  erase(pin);erase(confirmation);if(hooks.keyboard)hooks.keyboard(false,nullptr);
  completion=nullptr;completionContext=nullptr;overlay=pin=confirmation=status=submit=nullptr;
 },LV_EVENT_DELETE,nullptr);
 auto* box=lv_obj_create(overlay);const int width=std::min(660,static_cast<int>(lv_display_get_horizontal_resolution(nullptr))-32);
 lv_obj_set_size(box,width,initialize?390:290);lv_obj_align(box,LV_ALIGN_TOP_MID,0,52);
 lv_obj_set_style_pad_all(box,18,0);lv_obj_set_style_bg_color(box,color(1),0);lv_obj_set_style_text_color(box,color(2),0);
 if(hooks.font)lv_obj_set_style_text_font(box,hooks.font(),0);lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE);
 const int content=width-40;
 label(box,initialize?"Créer le PIN du coffre":"Déverrouiller le coffre",0,content);
 label(box,"PIN : 4 à 12 chiffres",38,content);pin=field(box,68,content);
 if(initialize){label(box,"Confirmer le PIN",130,content);confirmation=field(box,160,content);}
 const int y=initialize?226:136;
 button(box,"Annuler",0,y,(content-16)/2,false);
 submit=button(box,initialize?"Créer le PIN":"Déverrouiller",(content+16)/2,y,(content-16)/2,true);
 status=label(box,"",y+62,content);lv_obj_set_style_text_color(status,color(3),0);
 refresh();Tab5Focus.focusInitial(overlay,pin);if(hooks.keyboard)hooks.keyboard(!writing::usbConnected(),pin);
 return true;
}
void close(){finish(false);}
lv_obj_t* scope(){return overlay;}
bool key(const writing::KeyEvent& event){
 if(!overlay)return false;
 if(event.key==writing::Key::Escape){finish(false);return true;}
 auto* focused=Tab5Focus.focused();
 if(!Tab5Focus.contains(overlay,focused))Tab5Focus.focusInitial(overlay,pin);
 focused=Tab5Focus.focused();
 if(event.key==writing::Key::Enter&&!event.ctrl&&!event.meta&&!event.alt&&(focused==pin||focused==confirmation)){
  if(focused&&!lv_obj_has_state(focused,LV_STATE_DISABLED))ready(focused);return true;
 }
 Tab5Focus.key(event,overlay);return true;
}
void service(){
 if(!overlay)return;
 refresh();
 if(!pending)return;
 lv_label_set_text(status,tr(Tab5KeyStore::status()));
 if(Tab5KeyStore::busy())return;
 pending=false;Tab5KeyStore::finishUi();
 if(Tab5KeyStore::operationSucceeded()&&Tab5KeyStore::credentialsAccessible()){finish(true);return;}
 refresh();Tab5Focus.editObject(pin);if(hooks.keyboard)hooks.keyboard(!writing::usbConnected(),pin);
}
}
