#pragma once
#include <lvgl.h>
namespace Tab5MeshBadge {
lv_obj_t* create(lv_obj_t* parent,lv_event_cb_t clicked);
void update(bool active,bool ready,unsigned unread,const char* description);
lv_obj_t* object();
}
