#pragma once
#include "Tab5ShellPolicy.h"
#include <Preferences.h>
#include <lvgl.h>
namespace Tab5Shell {
static Choices choices;
struct Anchor {lv_obj_t* object=nullptr;int16_t offset=0;uint8_t divisor=1;bool position=false;};
// Pages have fewer than 100 explicit workspace anchors. Fixed storage avoids
// allocations during layout changes and deletion callbacks retire old objects.
static Anchor anchors[160];
static int workspaceWidth=1216;
inline void load(){Preferences p;if(p.begin("tab5sidebar",true)){Choices saved;if(p.getBytes("choices",&saved,sizeof(saved))==sizeof(saved)&&saved.valid())choices=saved;p.end();}}
inline bool save(){Preferences p;if(!p.begin("tab5sidebar",false))return false;bool ok=p.putBytes("choices",&choices,sizeof(choices))==sizeof(choices);p.end();return ok;}
inline void deleted(lv_event_t* event){auto* object=lv_event_get_target_obj(event);for(auto& entry:anchors)if(entry.object==object)entry.object=nullptr;}
inline void bind(lv_obj_t* object,int offset,unsigned divisor=1,bool position=false){
  if(!object)return;bool tracked=false;
  for(auto& entry:anchors)if(entry.object==object){tracked=true;if(entry.position==position){entry.offset=offset;entry.divisor=divisor;return;}}
  for(auto& entry:anchors)if(!entry.object){entry={object,int16_t(offset),uint8_t(divisor),position};if(!tracked)lv_obj_add_event_cb(object,deleted,LV_EVENT_DELETE,nullptr);return;}
}
inline void width(lv_obj_t* object,int offset=0,unsigned divisor=1){bind(object,offset,divisor);lv_obj_set_width(object,anchor(workspaceWidth,offset,divisor));}
inline void x(lv_obj_t* object,int offset,unsigned divisor=1){bind(object,offset,divisor,true);lv_obj_set_x(object,anchor(workspaceWidth,offset,divisor));}
inline void apply(int width){workspaceWidth=width;for(const auto& entry:anchors)if(entry.object){int value=anchor(width,entry.offset,entry.divisor);if(entry.position)lv_obj_set_x(entry.object,value);else lv_obj_set_width(entry.object,value);}}
}
