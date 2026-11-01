#include "Tab5UsbFormatUi.h"
#include "Tab5UsbStorage.h"
#include "Tab5UiStyle.h"
#include "Tab5Focus.h"
#include <cstdio>
#include <cstring>

namespace Tab5UsbFormatUi {
namespace {
Hooks hooks;
lv_obj_t *overlay=nullptr,*choice=nullptr,*action=nullptr,*cancel=nullptr,*status=nullptr;
uint32_t token=0;
Tab5UsbStorage::Format formats[2];
unsigned count=0;
bool submitted=false;
uint32_t submittedToken=0;
Tab5UsbStorage::Format submittedFormat=Tab5UsbStorage::Format::Fat32;
const char* tr(const char* s){return hooks.translate?hooks.translate(s):s;}
lv_color_t color(unsigned n){return lv_color_hex(hooks.color?hooks.color(n):(n==2?0xffffff:n==3?0xa0a0a0:0x202428));}
void visible(lv_obj_t* o,bool show){if(show)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
lv_obj_t* label(lv_obj_t* p,const char* text){auto* l=lv_label_create(p);lv_obj_set_width(l,lv_pct(100));lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);lv_label_set_text(l,tr(text));return l;}
lv_obj_t* button(lv_obj_t* p,const char* text,lv_event_cb_t cb){
  auto* b=lv_button_create(p);lv_obj_set_width(b,lv_pct(100));lv_obj_set_height(b,LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(b,48,0);lv_obj_set_style_pad_all(b,12,0);Tab5UiStyle::button(b);
  auto* l=label(b,text);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,nullptr);return b;
}
void note(const char* text){if(status&&strcmp(lv_label_get_text(status),tr(text)))lv_label_set_text(status,tr(text));}
void submit(lv_event_t*){
  if(submitted||!count||token!=Tab5UsbStorage::connectionToken())return;
  const auto selected=lv_dropdown_get_selected(choice);
  if(selected>=count)return;
  if(!Tab5UsbStorage::requestFormat(formats[selected],token)){note("Impossible de démarrer le formatage USB");return;}
  submittedToken=token;submittedFormat=formats[selected];
  submitted=true;lv_obj_add_state(choice,LV_STATE_DISABLED);service();
}
}
bool open(const Hooks& value){
  if(overlay)return true;
  if(!Tab5UsbStorage::attached())return false;
  hooks=value;token=Tab5UsbStorage::connectionToken();count=0;
  const auto current=Tab5UsbStorage::formatState();
  submitted=current==Tab5UsbStorage::FormatState::Waiting||current==Tab5UsbStorage::FormatState::Running;
  char options[16]{};
  if(Tab5UsbStorage::supported(Tab5UsbStorage::Format::Fat32)){formats[count++]=Tab5UsbStorage::Format::Fat32;strcpy(options,"FAT32");}
  if(Tab5UsbStorage::supported(Tab5UsbStorage::Format::Fat)){formats[count++]=Tab5UsbStorage::Format::Fat;if(*options)strcat(options,"\n");strcat(options,"FAT");}
  overlay=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(overlay);lv_obj_set_size(overlay,lv_pct(100),lv_pct(100));
  lv_obj_set_style_bg_color(overlay,color(0),0);lv_obj_set_style_bg_opa(overlay,LV_OPA_80,0);
  lv_obj_remove_flag(overlay,LV_OBJ_FLAG_SCROLLABLE);
  auto* card=lv_obj_create(overlay);lv_obj_set_width(card,lv_pct(78));lv_obj_set_height(card,lv_pct(90));lv_obj_center(card);
  lv_obj_set_style_bg_color(card,color(1),0);lv_obj_set_style_text_color(card,color(2),0);
  lv_obj_set_style_border_color(card,color(4),0);lv_obj_set_style_border_width(card,1,0);
  lv_obj_set_style_pad_all(card,20,0);lv_obj_set_style_pad_row(card,14,0);lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(card,LV_DIR_VER);if(hooks.font)lv_obj_set_style_text_font(card,hooks.font(),0);
  label(card,"Formater le support USB");
  char capacity[96];snprintf(capacity,sizeof(capacity),tr("USB · %llu Mio"),static_cast<unsigned long long>(Tab5UsbStorage::capacityBytes()/(1024u*1024u)));
  auto* details=label(card,"");lv_label_set_text(details,capacity);
  label(card,"Toutes les partitions et tous les fichiers du support USB seront effacés.");
  label(card,"Format");choice=lv_dropdown_create(card);lv_obj_set_width(choice,lv_pct(100));lv_obj_set_height(choice,48);
  lv_dropdown_set_options(choice,*options?options:tr("Aucun format compatible"));Tab5UiStyle::control(choice);
  if(submitted){
    lv_obj_add_state(choice,LV_STATE_DISABLED);
    if(submittedToken==token)for(unsigned i=0;i<count;++i)if(formats[i]==submittedFormat)lv_dropdown_set_selected(choice,i);
  }
  status=label(card,count?"Le formatage commence uniquement après confirmation.":"Capacité USB incompatible avec les formats disponibles");
  action=button(card,"Effacer et formater",submit);
  cancel=button(card,"Annuler le formatage",[](lv_event_t*){if(Tab5UsbStorage::cancelFormat())service();});
  button(card,"Fermer",[](lv_event_t*){close();});
  service();return true;
}
void close(){auto* old=overlay;overlay=choice=action=cancel=status=nullptr;if(old)lv_obj_delete(old);}
lv_obj_t* scope(){return overlay;}
bool key(const writing::KeyEvent& event){
  if(!overlay)return false;
  if(event.key==writing::Key::Escape){
    if(choice&&lv_dropdown_is_open(choice)){lv_dropdown_close(choice);Tab5Focus.finishEditing();}
    else close();
  }else Tab5Focus.key(event,overlay);
  return true;
}
void service(){
  if(!overlay)return;
  const bool same=Tab5UsbStorage::attached()&&token==Tab5UsbStorage::connectionToken();
  visible(action,!submitted);visible(cancel,submitted&&Tab5UsbStorage::formatState()==Tab5UsbStorage::FormatState::Waiting);
  if(same&&count&&!submitted)lv_obj_remove_state(action,LV_STATE_DISABLED);else lv_obj_add_state(action,LV_STATE_DISABLED);
  if(!same){note("Le support USB a changé ou a été retiré");return;}
  if(!submitted)return;
  using S=Tab5UsbStorage::FormatState;
  switch(Tab5UsbStorage::formatState()){
    case S::Waiting:note("Fermez les fichiers et applications utilisant le support USB pour commencer.");break;
    case S::Running:note("Formatage en cours… Ne retirez pas le support USB.");break;
    case S::Succeeded:note("Formatage USB terminé");break;
    case S::Failed:note(Tab5UsbStorage::status());break;
    case S::Cancelled:note("Formatage USB annulé");break;
    default:break;
  }
}
}
