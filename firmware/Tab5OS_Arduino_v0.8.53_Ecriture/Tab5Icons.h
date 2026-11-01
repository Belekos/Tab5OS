#pragma once
#include <lvgl.h>
#define TAB5_SYMBOL_PRINT "@print"
// Small native vector icon: follows the current theme without an extra font.
inline void tab5PrinterIcon(lv_obj_t* button,lv_color_t ink,lv_color_t paper) {
 auto* host=lv_obj_create(button);lv_obj_remove_style_all(host);lv_obj_set_size(host,32,32);lv_obj_center(host);lv_obj_remove_flag(host,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(host,LV_OBJ_FLAG_SCROLLABLE);
 auto rect=[&](int x,int y,int w,int h,lv_color_t fill,int border){auto* o=lv_obj_create(host);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,fill,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_border_color(o,ink,0);lv_obj_set_style_border_width(o,border,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);};
 rect(6,0,20,12,paper,2);rect(0,10,32,17,paper,2);rect(6,22,20,10,paper,2);rect(10,26,12,2,ink,0);rect(25,15,3,3,ink,0);
}
#define TAB5_SYMBOL_LOCK "@lock"
inline void tab5LockIcon(lv_obj_t* button,lv_color_t ink,lv_color_t paper) {
 auto* host=lv_obj_create(button);lv_obj_remove_style_all(host);lv_obj_set_size(host,32,32);lv_obj_center(host);lv_obj_remove_flag(host,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(host,LV_OBJ_FLAG_SCROLLABLE);
 auto rect=[&](int x,int y,int w,int h,int radius,lv_color_t fill){auto* o=lv_obj_create(host);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_radius(o,radius,0);lv_obj_set_style_bg_color(o,fill,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_border_color(o,ink,0);lv_obj_set_style_border_width(o,2,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);};
 rect(7,0,18,23,9,paper);rect(2,13,28,19,3,paper);rect(14,20,4,7,2,ink);
}

#define TAB5_SYMBOL_CALCULATOR "@calculator"
#define TAB5_SYMBOL_PYTHON "@python"
inline void tab5CalculatorIcon(lv_obj_t* host,lv_color_t ink) {
 auto* box=lv_obj_create(host);lv_obj_remove_style_all(box);lv_obj_set_size(box,28,36);lv_obj_center(box);
 lv_obj_set_style_border_width(box,2,0);lv_obj_set_style_border_color(box,ink,0);lv_obj_set_style_radius(box,3,0);
 lv_obj_remove_flag(box,lv_obj_flag_t(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
 auto rect=[&](int x,int y,int w,int h){auto* r=lv_obj_create(box);lv_obj_remove_style_all(r);lv_obj_set_pos(r,x,y);lv_obj_set_size(r,w,h);lv_obj_set_style_bg_color(r,ink,0);lv_obj_set_style_bg_opa(r,LV_OPA_COVER,0);lv_obj_remove_flag(r,lv_obj_flag_t(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));};
 rect(3,3,18,6);for(int y=0;y<3;++y)for(int x=0;x<3;++x)rect(3+x*7,13+y*6,4,3);
}
inline void tab5PythonIcon(lv_obj_t* host,lv_color_t ink) {
 auto* label=lv_label_create(host);lv_label_set_text(label,">_");lv_obj_set_style_text_font(label,&lv_font_montserrat_32,0);lv_obj_set_style_text_color(label,ink,0);lv_obj_center(label);
}

#define TAB5_SYMBOL_DRAW "@draw"
inline void tab5DrawingIcon(lv_obj_t* parent,lv_color_t ink){
 auto* host=lv_obj_create(parent);lv_obj_remove_style_all(host);lv_obj_set_size(host,32,32);lv_obj_center(host);
 lv_obj_set_clickable(host,false);lv_obj_set_scrollable(host,false);
 auto stroke=[&](const lv_point_precise_t* points,unsigned count,int width){
  auto* line=lv_line_create(host);lv_line_set_points(line,points,count);lv_obj_set_style_line_width(line,width,0);lv_obj_set_style_line_color(line,ink,0);
  lv_obj_set_clickable(line,false);lv_obj_set_scrollable(line,false);
 };
 static const lv_point_precise_t handle[]={{12,19},{23,3},{26,2},{29,5},{28,8},{17,23},{12,19}};
 static const lv_point_precise_t ferrule[]={{11,20},{16,24}};
 static const lv_point_precise_t bristles[]={{11,21},{7,22},{5,26},{2,29},{7,30},{11,29},{14,26},{14,24}};
 stroke(handle,7,2);stroke(ferrule,2,3);stroke(bristles,8,2);
}

#define TAB5_SYMBOL_ALARM "@alarm"
inline void tab5AlarmIcon(lv_obj_t* parent,lv_color_t ink,lv_color_t paper){
 auto* host=lv_obj_create(parent);lv_obj_remove_style_all(host);lv_obj_set_size(host,36,36);lv_obj_center(host);
 lv_obj_set_clickable(host,false);lv_obj_set_scrollable(host,false);
 auto stroke=[&](const lv_point_precise_t* points,unsigned count,int width){
  auto* line=lv_line_create(host);lv_line_set_points(line,points,count);lv_obj_set_style_line_width(line,width,0);lv_obj_set_style_line_color(line,ink,0);
  lv_obj_set_clickable(line,false);lv_obj_set_scrollable(line,false);
 };
 static const lv_point_precise_t leftBell[]={{2,9},{3,5},{6,2},{10,1},{13,3},{14,5},{2,9}};
 static const lv_point_precise_t rightBell[]={{22,5},{23,3},{26,1},{30,2},{33,5},{34,9},{22,5}};
 static const lv_point_precise_t leftFoot[]={{10,29},{6,34}},rightFoot[]={{26,29},{30,34}};
 stroke(leftBell,7,2);stroke(rightBell,7,2);stroke(leftFoot,2,3);stroke(rightFoot,2,3);
 auto* face=lv_obj_create(host);lv_obj_remove_style_all(face);lv_obj_set_pos(face,5,7);lv_obj_set_size(face,26,26);lv_obj_set_style_radius(face,LV_RADIUS_CIRCLE,0);
 lv_obj_set_style_bg_color(face,paper,0);lv_obj_set_style_bg_opa(face,LV_OPA_COVER,0);lv_obj_set_style_border_width(face,2,0);lv_obj_set_style_border_color(face,ink,0);
 lv_obj_set_clickable(face,false);lv_obj_set_scrollable(face,false);
 static const lv_point_precise_t hands[]={{12,15},{18,21},{23,14}};
 stroke(hands,3,2);
 auto* center=lv_obj_create(host);lv_obj_remove_style_all(center);lv_obj_set_pos(center,16,19);lv_obj_set_size(center,4,4);lv_obj_set_style_radius(center,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_color(center,ink,0);lv_obj_set_style_bg_opa(center,LV_OPA_COVER,0);
 lv_obj_set_clickable(center,false);lv_obj_set_scrollable(center,false);
}
