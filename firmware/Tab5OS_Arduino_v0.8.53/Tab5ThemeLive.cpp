#include "Tab5ThemeLive.h"
#include <lvgl_private.h>
#include <new>
#include <vector>

namespace Tab5ThemeLive {
namespace {
struct Binding {
  lv_style_selector_t selector;
  lv_style_prop_t property;
  uint32_t last;
  uint8_t role;
  bool explicitRole=false;
};
struct Bindings {std::vector<Binding> values;};
char preserveTag;
void preserveMarker(lv_event_t*) {}

void destroyed(lv_event_t* e) {
  if(lv_event_get_target_obj(e)==lv_event_get_current_target_obj(e))
    delete static_cast<Bindings*>(lv_event_get_user_data(e));
}
Bindings* bindings(lv_obj_t* obj) {
  for(uint32_t i=0;i<lv_obj_get_event_count(obj);++i) {
    auto* descriptor=lv_obj_get_event_dsc(obj,i);
    if(lv_event_dsc_get_cb(descriptor)==destroyed)
      return static_cast<Bindings*>(lv_event_dsc_get_user_data(descriptor));
  }
  return nullptr;
}
int roleFor(lv_obj_t* obj,lv_style_prop_t property,lv_style_selector_t selector,
            uint32_t value,const uint32_t* old) {
  // Prefer the semantic role when a saved palette deliberately repeats colours.
  int preferred=-1;
  if(property==LV_STYLE_BORDER_COLOR)preferred=3;
  else if(property==LV_STYLE_TEXT_COLOR)preferred=4;
  else if(property==LV_STYLE_OUTLINE_COLOR)preferred=7;
  else if((selector&LV_PART_ANY)==LV_PART_CURSOR)preferred=7;
  else if((selector&LV_PART_ANY)==LV_PART_SELECTED)preferred=8;
  else if((selector&LV_STATE_CHECKED)||(selector&LV_STATE_PRESSED))preferred=6;
  else if(property==LV_STYLE_BG_COLOR)
    preferred=lv_obj_get_style_radius(obj,LV_PART_MAIN)>0?1:0;
  if(preferred>=0&&old[preferred]==value)return preferred;
  for(unsigned i=0;i<9;++i)if(old[i]==value)return int(i);
  return -1;
}
void visit(lv_obj_t* obj,const uint32_t* old,const uint32_t* next) {
  if(!obj||preserved(obj))return;
  Bindings* saved=bindings(obj);
  const lv_style_prop_t properties[]={LV_STYLE_BG_COLOR,LV_STYLE_BG_GRAD_COLOR,
    LV_STYLE_BORDER_COLOR,LV_STYLE_OUTLINE_COLOR,LV_STYLE_SHADOW_COLOR,
    LV_STYLE_TEXT_COLOR,LV_STYLE_LINE_COLOR,LV_STYLE_ARC_COLOR,
    LV_STYLE_IMAGE_RECOLOR,LV_STYLE_BG_IMAGE_RECOLOR};
  // LVGL 9 keeps selector metadata alongside each style. Snapshot selectors
  // before calling the public setter so no style-array pointer survives it.
  lv_style_selector_t selectors[64];unsigned count=0;
  for(unsigned i=0;i<obj->style_cnt&&count<64;++i)
    if(obj->styles[i].is_local&&!obj->styles[i].is_trans)
      selectors[count++]=obj->styles[i].selector;
  for(unsigned i=0;i<count;++i)for(auto property:properties) {
    lv_style_value_t value;
    if(lv_obj_get_local_style_prop(obj,property,&value,selectors[i])!=LV_STYLE_RES_FOUND)continue;
    const uint32_t rgb=lv_color_to_u32(value.color)&0xffffff;
    Binding* known=nullptr;
    if(saved)for(auto& b:saved->values)
      if(b.selector==selectors[i]&&b.property==property){known=&b;break;}
    const int role=known&&(known->explicitRole||known->last==rgb)?int(known->role):roleFor(obj,property,selectors[i],rgb,old);
    if(role<0)continue; // e.g. syntax highlighting, cover art or status colours
    if(!saved) {
      saved=new(std::nothrow) Bindings;
      if(saved)lv_obj_add_event_cb(obj,destroyed,LV_EVENT_DELETE,saved);
    }
    if(saved&&!known) {
      saved->values.push_back({selectors[i],property,rgb,uint8_t(role)});
      known=&saved->values.back();
    }
    if(known){known->role=uint8_t(role);known->last=next[role];}
    if(rgb!=next[role]) {
      value.color=lv_color_hex(next[role]);
      lv_obj_set_local_style_prop(obj,property,value,selectors[i]);
    }
  }
  for(uint32_t i=0;i<lv_obj_get_child_count(obj);++i)visit(lv_obj_get_child(obj,i),old,next);
}
}
void bind(lv_obj_t* obj,lv_style_prop_t property,lv_style_selector_t selector,unsigned role) {
  if(!obj||role>=9)return;
  lv_style_value_t value;
  if(lv_obj_get_local_style_prop(obj,property,&value,selector)!=LV_STYLE_RES_FOUND)return;
  const uint32_t rgb=lv_color_to_u32(value.color)&0xffffff;
  auto* saved=bindings(obj);
  if(!saved){saved=new(std::nothrow) Bindings;if(!saved)return;if(!lv_obj_add_event_cb(obj,destroyed,LV_EVENT_DELETE,saved)){delete saved;return;}}
  for(auto& b:saved->values)if(b.selector==selector&&b.property==property){b.last=rgb;b.role=uint8_t(role);b.explicitRole=true;return;}
  saved->values.push_back({selector,property,rgb,uint8_t(role),true});
}
bool preserved(lv_obj_t* obj) {
  if(!obj)return false;
  // Linkers may fold identical empty callbacks; the unique tag keeps this
  // marker distinct from the other style markers even in optimized builds.
  for(uint32_t i=0;i<lv_obj_get_event_count(obj);++i) {
    auto* descriptor=lv_obj_get_event_dsc(obj,i);
    if(lv_event_dsc_get_cb(descriptor)==preserveMarker&&
       lv_event_dsc_get_user_data(descriptor)==&preserveTag)return true;
  }
  return false;
}
void preserve(lv_obj_t* obj) {
  if(obj&&!preserved(obj))lv_obj_add_event_cb(obj,preserveMarker,LV_EVENT_DELETE,&preserveTag);
}
void apply(lv_obj_t* root,const uint32_t* oldPalette,const uint32_t* newPalette) {
  if(!root||!oldPalette||!newPalette)return;
  visit(root,oldPalette,newPalette);
  lv_obj_invalidate(root);
}
}
