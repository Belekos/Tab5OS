#pragma once
#include <stdint.h>
#include <map>
#include <vector>
using lv_style_selector_t=uint32_t;
using lv_style_prop_t=uint16_t;
using lv_color_t=uint32_t;
struct lv_style_value_t {lv_color_t color;};
enum {LV_STYLE_BG_COLOR,LV_STYLE_BG_GRAD_COLOR,LV_STYLE_BORDER_COLOR,
 LV_STYLE_OUTLINE_COLOR,LV_STYLE_SHADOW_COLOR,LV_STYLE_TEXT_COLOR,
 LV_STYLE_LINE_COLOR,LV_STYLE_ARC_COLOR,LV_STYLE_IMAGE_RECOLOR,LV_STYLE_BG_IMAGE_RECOLOR};
enum {LV_PART_MAIN=0,LV_PART_CURSOR=0x60000,LV_PART_SELECTED=0x70000,LV_PART_ANY=0xff0000,
 LV_STATE_CHECKED=1,LV_STATE_PRESSED=0x20,LV_STATE_FOCUSED=2,LV_EVENT_DELETE=1,
 LV_STYLE_RES_FOUND=1,LV_STYLE_RES_NOT_FOUND=0};
struct lv_obj_t;
struct lv_event_t {lv_obj_t* target;void* data;};
using lv_event_cb_t=void(*)(lv_event_t*);
struct lv_event_dsc_t {lv_event_cb_t callback;void* data;};
struct lv_obj_style_t {lv_style_selector_t selector;bool is_local=true,is_trans=false;};
struct lv_obj_t {
 lv_obj_style_t styles[64];unsigned style_cnt=0;
 std::map<uint64_t,lv_style_value_t> values;
 std::vector<lv_event_dsc_t> events;
 std::vector<lv_obj_t*> children;
 int radius=0,scroll=71,cursor=4;bool hidden=false;unsigned invalidate=0;
 const char* content="Unsaved draft";
 ~lv_obj_t(){for(auto e:events){lv_event_t event{this,e.data};e.callback(&event);}for(auto* c:children)delete c;}
};
inline lv_obj_t* lv_event_get_target_obj(lv_event_t* e){return e->target;}
inline lv_obj_t* lv_event_get_current_target_obj(lv_event_t* e){return e->target;}
inline void* lv_event_get_user_data(lv_event_t* e){return e->data;}
inline unsigned lv_obj_get_event_count(lv_obj_t* o){return unsigned(o->events.size());}
inline lv_event_dsc_t* lv_obj_get_event_dsc(lv_obj_t* o,unsigned i){return &o->events.at(i);}
inline lv_event_cb_t lv_event_dsc_get_cb(lv_event_dsc_t* d){return d->callback;}
inline void* lv_event_dsc_get_user_data(lv_event_dsc_t* d){return d->data;}
inline void lv_obj_add_event_cb(lv_obj_t* o,lv_event_cb_t callback,unsigned,void* data){o->events.push_back({callback,data});}
inline int lv_obj_get_style_radius(lv_obj_t* o,unsigned){return o->radius;}
inline uint32_t lv_color_to_u32(lv_color_t v){return v;}
inline lv_color_t lv_color_hex(uint32_t v){return v;}
inline int lv_obj_get_local_style_prop(lv_obj_t* o,lv_style_prop_t p,lv_style_value_t* v,lv_style_selector_t s){auto i=o->values.find((uint64_t(s)<<16)|p);if(i==o->values.end())return LV_STYLE_RES_NOT_FOUND;*v=i->second;return LV_STYLE_RES_FOUND;}
inline void lv_obj_set_local_style_prop(lv_obj_t* o,lv_style_prop_t p,lv_style_value_t v,lv_style_selector_t s){bool seen=false;for(unsigned i=0;i<o->style_cnt;++i)if(o->styles[i].selector==s)seen=true;if(!seen)o->styles[o->style_cnt++]={s,true,false};o->values[(uint64_t(s)<<16)|p]=v;}
inline unsigned lv_obj_get_child_count(lv_obj_t* o){return unsigned(o->children.size());}
inline lv_obj_t* lv_obj_get_child(lv_obj_t* o,unsigned i){return o->children.at(i);}
inline void lv_obj_invalidate(lv_obj_t* o){++o->invalidate;}
