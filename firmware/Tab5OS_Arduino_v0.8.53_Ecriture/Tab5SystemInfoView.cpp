#include "Tab5SystemInfoView.h"
#include "Tab5UiStyle.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

namespace Tab5SystemInfoView {
namespace {
Hooks hooks;
lv_obj_t *overlay=nullptr,*content=nullptr,*heading=nullptr,*hardware=nullptr,*power=nullptr,*connections=nullptr,*display=nullptr,*detailText=nullptr,*detailButton=nullptr,*memoryLayout=nullptr;
lv_obj_t *bars[Count]{},*values[Count]{},*notes[Count]{};
bool expanded=false;
// LVGL stores shaped Arabic label text. Compare the original snapshot instead
// of reshaping on every sample: that would allocate temporary buffers and
// invalidate unchanged labels. This cache exists only while the popup is open,
// in the same PSRAM-backed allocator as the UI. OOM preserves normal rendering.
Snapshot* previous=nullptr;
bool previousValid=false;
const char* tr(const char* s){return hooks.translate?hooks.translate(s):s;}
lv_color_t color(unsigned n){return lv_color_hex(Tab5UiStyle::color(n));}
void text(lv_obj_t* o,const char* s){if(o&&strcmp(lv_label_get_text(o),s))lv_label_set_text(o,s);}
lv_obj_t* label(lv_obj_t* p,const char* s){auto* o=lv_label_create(p);lv_obj_set_width(o,lv_pct(100));lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_label_set_text(o,tr(s));return o;}
lv_obj_t* column(lv_obj_t* p){auto* o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_size(o,lv_pct(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(o,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(o,8,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;}
lv_obj_t* card(lv_obj_t* p,const char* title){auto* o=column(p);lv_obj_set_style_pad_all(o,14,0);lv_obj_set_style_bg_color(o,color(1),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_border_color(o,color(3),0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_radius(o,Tab5UiStyle::radius(),0);if(title&&*title){auto* t=label(o,title);lv_obj_set_style_text_color(t,color(5),0);}return o;}
lv_obj_t* button(lv_obj_t* p,const char* s,lv_event_cb_t cb){auto* o=lv_button_create(p);lv_obj_set_size(o,LV_SIZE_CONTENT,48);lv_obj_set_style_min_width(o,144,0);lv_obj_set_style_pad_hor(o,16,0);Tab5UiStyle::button(o);auto* l=lv_label_create(o);lv_label_set_text(l,tr(s));lv_obj_center(l);lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,nullptr);return o;}
void toggle(){expanded=!expanded;if(expanded)lv_obj_remove_flag(detailText,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(detailText,LV_OBJ_FLAG_HIDDEN);text(lv_obj_get_child(detailButton,0),tr(expanded?"Masquer les détails":"Détails techniques"));if(hooks.refresh)hooks.refresh();}
}
void open(Hooks value){
  if(overlay)return;hooks=value;expanded=false;
  if(void* storage=lv_malloc(sizeof(Snapshot)))previous=new(storage) Snapshot;
  previousValid=false;
  overlay=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(overlay);lv_obj_set_size(overlay,lv_pct(100),lv_pct(100));lv_obj_set_style_bg_color(overlay,lv_color_black(),0);lv_obj_set_style_bg_opa(overlay,LV_OPA_70,0);lv_obj_remove_flag(overlay,LV_OBJ_FLAG_SCROLLABLE);
  auto* box=column(overlay);lv_obj_set_size(box,lv_pct(92),lv_pct(93));lv_obj_center(box);lv_obj_set_style_pad_all(box,18,0);lv_obj_set_style_bg_color(box,color(0),0);lv_obj_set_style_bg_opa(box,LV_OPA_COVER,0);lv_obj_set_style_text_color(box,color(4),0);lv_obj_set_style_radius(box,Tab5UiStyle::radius(),0);if(hooks.font)lv_obj_set_style_text_font(box,hooks.font(),0);
  heading=label(box,"Informations système");
  content=column(box);lv_obj_set_height(content,0);lv_obj_set_flex_grow(content,1);lv_obj_add_flag(content,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(content,LV_DIR_VER);lv_obj_set_scrollbar_mode(content,LV_SCROLLBAR_MODE_AUTO);lv_obj_set_style_pad_right(content,10,0);
  auto* meters=column(content);lv_obj_set_flex_flow(meters,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(meters,12,0);
  const char* titles[]={"SRAM HP · tas","PSRAM · tas","Firmware · partition flash"};
  for(unsigned i=0;i<Count;++i){auto* c=card(meters,titles[i]);lv_obj_set_width(c,0);lv_obj_set_flex_grow(c,1);values[i]=label(c,"");bars[i]=lv_bar_create(c);lv_obj_set_size(bars[i],lv_pct(100),14);lv_bar_set_range(bars[i],0,1000);Tab5UiStyle::control(bars[i]);notes[i]=label(c,"");lv_obj_set_style_text_color(notes[i],color(5),0);}
  memoryLayout=label(card(content,"Mémoire du P4"),"");
  auto* row=column(content);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(row,12,0);
  auto* left=column(row);auto* right=column(row);for(auto* o:{left,right}){lv_obj_set_width(o,0);lv_obj_set_flex_grow(o,1);}
  hardware=label(card(left,"Matériel"),"");power=label(card(left,"Alimentation"),"");connections=label(card(right,"Connexions et stockage"),"");display=label(card(right,"Affichage"),"");
  detailText=label(content,"");Tab5TextInput::selectable(detailText);lv_obj_add_flag(detailText,LV_OBJ_FLAG_HIDDEN);
  auto* actions=column(box);lv_obj_set_flex_flow(actions,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(actions,10,0);
  button(actions,"Actualiser",[](lv_event_t*){if(hooks.refresh)hooks.refresh();});
  detailButton=button(actions,"Détails techniques",[](lv_event_t*){toggle();});
  button(actions,"Retester la SD",[](lv_event_t*){if(hooks.retestSd)hooks.retestSd();});
  auto* spacer=column(actions);lv_obj_set_width(spacer,0);lv_obj_set_flex_grow(spacer,1);
  auto* done=button(actions,"Fermer",[](lv_event_t*){close();});
  Tab5Focus.focusObject(done);if(hooks.refresh)hooks.refresh();
}
void close(){if(!overlay)return;Tab5Focus.clear();auto* old=overlay;overlay=content=heading=hardware=power=connections=display=detailText=detailButton=memoryLayout=nullptr;for(unsigned i=0;i<Count;++i)bars[i]=values[i]=notes[i]=nullptr;expanded=false;if(previous){previous->~Snapshot();lv_free(previous);previous=nullptr;}previousValid=false;lv_obj_delete(old);if(hooks.closed)hooks.closed();}
void update(const Snapshot& s){
  if(!overlay)return;
  const auto changed=[&](const char* now,const char* before){return !previousValid||strcmp(now,before);};
  if(changed(s.heading,previous?previous->heading:""))text(heading,s.heading);
  if(changed(s.hardware,previous?previous->hardware:""))text(hardware,s.hardware);
  if(changed(s.power,previous?previous->power:""))text(power,s.power);
  if(changed(s.connections,previous?previous->connections:""))text(connections,s.connections);
  if(changed(s.display,previous?previous->display:""))text(display,s.display);
  if(changed(s.memoryLayout,previous?previous->memoryLayout:""))text(memoryLayout,s.memoryLayout);
  for(unsigned i=0;i<Count;++i){const auto& m=s.meters[i];
    if(!previousValid||m.used!=previous->meters[i].used||m.total!=previous->meters[i].total){
      char value[128];const uint64_t used=std::min(m.used,m.total);const unsigned amount=m.total?unsigned(double(used)*1000.0/double(m.total)):0;
      if(m.total){if(i==Firmware)snprintf(value,sizeof(value),tr("%.2f / %.2f Mio occupés"),double(used)/(1024*1024),double(m.total)/(1024*1024));else if(m.total>=1024*1024)snprintf(value,sizeof(value),tr("%.1f / %.1f Mio utilisés"),double(used)/(1024*1024),double(m.total)/(1024*1024));else snprintf(value,sizeof(value),tr("%u / %u Kio utilisés"),unsigned(used/1024),unsigned(m.total/1024));}
      else snprintf(value,sizeof(value),"%s",tr("Indisponible"));
      text(values[i],value);if(lv_bar_get_value(bars[i])!=int(amount))lv_bar_set_value(bars[i],amount,LV_ANIM_OFF);
    }
    if(changed(m.note,previous?previous->meters[i].note:""))text(notes[i],m.note);
  }
  if(previous){*previous=s;previousValid=true;}
}
void details(const char* s){if(expanded)text(detailText,s?s:"");}
bool wantsDetails(){return expanded;}
lv_obj_t* scope(){return overlay;}
lv_obj_t* body(){return content;}
}
