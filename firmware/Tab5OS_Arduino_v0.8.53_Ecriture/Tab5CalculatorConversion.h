#pragma once
#include "Tab5CalculatorHistory.h"

namespace tab5calc {
class ConversionWindow {
 public:
 struct Hooks {
  uint32_t(*color)(unsigned,void*)=nullptr;const char*(*translate)(const char*,void*)=nullptr;
  void(*style)(lv_obj_t*,void*)=nullptr;void(*edit)(lv_obj_t*,void*)=nullptr;
  void(*use)(double,void*)=nullptr;void(*closed)(void*)=nullptr;void* context=nullptr;
 };
 lv_obj_t* scope()const{return overlay_;}lv_obj_t* input()const{return value_;}
 lv_obj_t* output()const{return result_;}lv_obj_t* from()const{return from_;}lv_obj_t* to()const{return to_;}
 lv_obj_t* useButton()const{return use_;}
 void open(lv_obj_t* parent,const char* expression,bool degrees,double answer,double memory,Hooks hooks){
  close(false);hooks_=hooks;degrees_=degrees;answer_=answer;memory_=memory;
  overlay_=lv_obj_create(parent);lv_obj_set_size(overlay_,lv_pct(100),lv_pct(100));lv_obj_set_pos(overlay_,0,0);lv_obj_set_style_pad_all(overlay_,0,0);lv_obj_set_style_radius(overlay_,0,0);lv_obj_set_style_border_width(overlay_,0,0);lv_obj_set_style_bg_color(overlay_,lv_color_hex(color(0)),0);lv_obj_set_style_bg_opa(overlay_,LV_OPA_80,0);lv_obj_remove_flag(overlay_,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(overlay_,[](lv_event_t* e){auto* self=static_cast<ConversionWindow*>(lv_event_get_user_data(e));if(lv_event_get_target_obj(e)==self->overlay_){self->overlay_=self->panel_=self->value_=self->result_=self->from_=self->to_=self->use_=nullptr;self->valid_=false;}},LV_EVENT_DELETE,this);
  panel_=lv_obj_create(overlay_);lv_obj_set_size(panel_,std::min(680,int(lv_obj_get_width(parent))-32),std::min(488,int(lv_obj_get_height(parent))-24));lv_obj_center(panel_);lv_obj_set_style_pad_all(panel_,16,0);lv_obj_set_style_bg_color(panel_,lv_color_hex(color(1)),0);lv_obj_set_style_border_color(panel_,lv_color_hex(color(4)),0);lv_obj_set_style_text_color(panel_,lv_color_hex(color(2)),0);lv_obj_set_scroll_dir(panel_,LV_DIR_VER);lv_obj_remove_flag(panel_,LV_OBJ_FLAG_SCROLL_CHAIN);lv_obj_update_layout(panel_);const int width=lv_obj_get_content_width(panel_);
  label("Conversions",0,0,width-72);button(LV_SYMBOL_CLOSE,width-48,0,48,0);
  label("Valeur",0,60,width);value_=field(expression&&*expression?expression:"0",0,96,width,false);
  label("De",0,160,(width-44)/2);label("Vers",(width+44)/2,160,(width-44)/2);
  from_=dropdown(0,196,(width-44)/2);to_=dropdown((width+44)/2,196,(width-44)/2);lv_dropdown_set_selected(to_,2);
  auto* arrow=label(LV_SYMBOL_RIGHT,(width-28)/2,202,28);lv_obj_set_style_text_font(arrow,&lv_font_montserrat_24,0);
  label("Résultat",0,256,width);result_=field("",0,292,width,true);convert_=button("Convertir",0,364,(width-16)/2,1);use_=button("Utiliser",(width+16)/2,364,(width-16)/2,2);lv_obj_add_state(use_,LV_STATE_DISABLED);
  for(auto* object:{value_,from_,to_})lv_obj_add_event_cb(object,[](lv_event_t* e){auto* self=static_cast<ConversionWindow*>(lv_event_get_user_data(e));self->valid_=false;lv_obj_add_state(self->use_,LV_STATE_DISABLED);lv_textarea_set_text(self->result_,"");},LV_EVENT_VALUE_CHANGED,this);
  Tab5Focus.focusInitial(panel_,value_);
 }
 void close(bool notify=true){auto* old=overlay_;if(!old)return;lv_obj_delete(old);if(notify&&hooks_.closed)hooks_.closed(hooks_.context);}
 void calculate(){
  if(!overlay_)return;auto value=evaluate(lv_textarea_get_text(value_),degrees_,answer_,memory_);Result converted=value?convert(value.value,lv_dropdown_get_selected(from_),lv_dropdown_get_selected(to_)):value;
  valid_=bool(converted);if(!converted){lv_textarea_set_text(result_,tr(converted.error));lv_obj_add_state(use_,LV_STATE_DISABLED);return;}
  converted_=converted.value;char text[32];History::format(converted_,text,sizeof(text));lv_textarea_set_text(result_,text);lv_obj_remove_state(use_,LV_STATE_DISABLED);
 }
 bool key(const writing::KeyEvent& e){
  if(!overlay_)return false;using K=writing::Key;
  if(e.key==K::Escape){for(auto* dropdown:{from_,to_})if(lv_dropdown_is_open(dropdown)){lv_dropdown_close(dropdown);Tab5Focus.finishEditing();Tab5Focus.focusObject(dropdown);return true;}close();return true;}
  if(e.key==K::Enter&&((e.ctrl||e.meta)||Tab5Focus.focused()==value_)){calculate();return true;}
  auto* focused=Tab5Focus.focused();if(focused&&lv_obj_check_type(focused,&lv_textarea_class)&&(e.ctrl||e.meta||(e.shift&&(e.key==K::Insert||e.key==K::Delete)))){Tab5TextInput::key(focused,e);return true;}
  Tab5Focus.key(e,overlay_);return true;
 }
 private:
 const char* tr(const char* text){return hooks_.translate?hooks_.translate(text,hooks_.context):text;}
 uint32_t color(unsigned role){return hooks_.color?hooks_.color(role,hooks_.context):role==2?0xffffff:0x202428;}
 void style(lv_obj_t* object){if(hooks_.style)hooks_.style(object,hooks_.context);}
 lv_obj_t* label(const char* text,int x,int y,int width){auto* object=lv_label_create(panel_);lv_label_set_text(object,tr(text));lv_obj_set_pos(object,x,y);lv_obj_set_width(object,width);return object;}
 lv_obj_t* field(const char* text,int x,int y,int width,bool readOnly){auto* object=lv_textarea_create(panel_);lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,52);lv_textarea_set_one_line(object,true);lv_textarea_set_max_length(object,2048);lv_textarea_set_text_selection(object,true);lv_textarea_set_text(object,text);lv_obj_set_style_text_color(object,lv_color_hex(color(2)),0);style(object);if(readOnly)Tab5TextInput::setReadOnly(object);else Tab5TextInput::track(object);
  lv_obj_add_event_cb(object,[](lv_event_t* e){auto* self=static_cast<ConversionWindow*>(lv_event_get_user_data(e));if(self->hooks_.edit)self->hooks_.edit(lv_event_get_target_obj(e),self->hooks_.context);},LV_EVENT_CLICKED,this);return object;}
 lv_obj_t* dropdown(int x,int y,int width){auto* object=lv_dropdown_create(panel_);lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,52);lv_dropdown_clear_options(object);size_t count;const auto* list=units(count);for(size_t i=0;i<count;++i)lv_dropdown_add_option(object,list[i].name,LV_DROPDOWN_POS_LAST);style(object);return object;}
 lv_obj_t* button(const char* text,int x,int y,int width,int action){auto* object=lv_button_create(panel_);lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,52);lv_obj_set_style_bg_color(object,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(object,lv_color_hex(color(2)),0);lv_obj_set_style_border_color(object,lv_color_hex(color(4)),0);lv_obj_set_style_border_width(object,1,0);lv_obj_set_style_shadow_width(object,0,0);style(object);auto* caption=lv_label_create(object);lv_label_set_text(caption,tr(text));lv_obj_center(caption);if(!action)lv_obj_set_style_text_font(caption,&lv_font_montserrat_24,0);lv_obj_set_user_data(object,reinterpret_cast<void*>(intptr_t(action)));
  lv_obj_add_event_cb(object,[](lv_event_t* e){auto* self=static_cast<ConversionWindow*>(lv_event_get_user_data(e));const auto action=intptr_t(lv_obj_get_user_data(lv_event_get_target_obj(e)));if(action==0)self->close();else if(action==1)self->calculate();else if(self->valid_){const double value=self->converted_;self->close();if(self->hooks_.use)self->hooks_.use(value,self->hooks_.context);}},LV_EVENT_CLICKED,this);return object;}
 Hooks hooks_;lv_obj_t *overlay_=nullptr,*panel_=nullptr,*value_=nullptr,*result_=nullptr,*from_=nullptr,*to_=nullptr,*convert_=nullptr,*use_=nullptr;bool degrees_=true,valid_=false;double answer_=0,memory_=0,converted_=0;
};
}
