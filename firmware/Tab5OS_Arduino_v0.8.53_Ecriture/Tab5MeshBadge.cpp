#include "Tab5MeshBadge.h"
#include "Tab5UiStyle.h"
#include "Tab5Focus.h"
namespace Tab5MeshBadge {
namespace {
lv_obj_t* badge=nullptr;bool enabled=false,prepared=false,mail=false;
void draw(lv_event_t* e){
 auto* layer=lv_event_get_layer(e);lv_area_t a;lv_obj_get_coords(badge,&a);
 const auto color=lv_color_hex(enabled?0x299956:Tab5UiStyle::color(5));
 lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.width=2;line.color=color;
 auto segment=[&](int x1,int y1,int x2,int y2){line.p1={lv_value_precise_t(a.x1+x1),lv_value_precise_t(a.y1+y1)};line.p2={lv_value_precise_t(a.x1+x2),lv_value_precise_t(a.y1+y2)};lv_draw_line(layer,&line);};
 segment(12,23,23,11);segment(23,11,31,24);segment(12,23,31,24);
 lv_draw_rect_dsc_t dot;lv_draw_rect_dsc_init(&dot);dot.radius=LV_RADIUS_CIRCLE;dot.bg_color=color;dot.bg_opa=LV_OPA_COVER;
 const lv_point_t points[]={{12,23},{23,11},{31,24}};
 for(const auto& p:points){lv_area_t area={a.x1+p.x-3,a.y1+p.y-3,a.x1+p.x+3,a.y1+p.y+3};lv_draw_rect(layer,&dot,&area);}
 if(mail){lv_draw_rect_dsc_t box;lv_draw_rect_dsc_init(&box);box.bg_opa=LV_OPA_TRANSP;box.border_width=2;box.border_color=lv_color_hex(Tab5UiStyle::color(4));box.radius=2;lv_area_t area={a.x1+39,a.y1+11,a.x1+57,a.y1+25};lv_draw_rect(layer,&box,&area);line.color=box.border_color;segment(40,12,48,19);segment(48,19,56,12);}
}
}
lv_obj_t* create(lv_obj_t* parent,lv_event_cb_t clicked){
 badge=lv_button_create(parent);lv_obj_set_size(badge,64,36);lv_obj_set_style_pad_all(badge,0,0);Tab5UiStyle::button(badge);
 lv_obj_add_flag(badge,LV_OBJ_FLAG_HIDDEN);
 lv_obj_add_event_cb(badge,draw,LV_EVENT_DRAW_MAIN_END,nullptr);if(clicked)lv_obj_add_event_cb(badge,clicked,LV_EVENT_CLICKED,nullptr);
 lv_obj_add_event_cb(badge,[](lv_event_t*){badge=nullptr;},LV_EVENT_DELETE,nullptr);return badge;
}
void update(bool active,bool ready,unsigned unread,const char* description){
 if(!badge)return;const bool nextMail=unread!=0;
 if(active){if(lv_obj_has_flag(badge,LV_OBJ_FLAG_HIDDEN))lv_obj_remove_flag(badge,LV_OBJ_FLAG_HIDDEN);}
 else if(!lv_obj_has_flag(badge,LV_OBJ_FLAG_HIDDEN))lv_obj_add_flag(badge,LV_OBJ_FLAG_HIDDEN);
 if(active!=enabled||ready!=prepared||nextMail!=mail){enabled=active;prepared=ready;mail=nextMail;if(active)lv_obj_invalidate(badge);}
 if(description)Tab5Focus.describe(badge,description);
}
lv_obj_t* object(){return badge;}
}
