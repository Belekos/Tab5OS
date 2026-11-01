#pragma once
#include <cstdint>
#include <cstdlib>
struct lv_obj_t{};struct lv_event_t{};
struct lv_image_dsc_t{struct {unsigned magic=0,cf=0,w=0,h=0,stride=0;} header;unsigned data_size=0;uint8_t* data=nullptr;};
constexpr int LV_IMAGE_HEADER_MAGIC=0x19,LV_COLOR_FORMAT_ARGB8888=1,LV_EVENT_DELETE=1;
inline void* lv_event_get_user_data(lv_event_t*){return nullptr;}
inline void lv_obj_clean(lv_obj_t*){}
inline lv_obj_t* lv_image_create(lv_obj_t*){return nullptr;}
inline void lv_image_set_src(lv_obj_t*,const void*){}
inline void lv_image_set_scale(lv_obj_t*,unsigned){}
inline void lv_obj_center(lv_obj_t*){}
inline void lv_obj_add_event_cb(lv_obj_t*,void(*)(lv_event_t*),int,void*){}
inline void lv_image_cache_drop(const void*){}
inline void lv_image_header_cache_drop(const void*){}
