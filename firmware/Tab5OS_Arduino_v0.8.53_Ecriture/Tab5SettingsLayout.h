#pragma once
#include <lvgl.h>
#include <initializer_list>

// Geometry only: values, callbacks and focus ownership stay with the page.
// Content height follows translated labels; controls keep 48 px touch rows.
namespace Tab5SettingsLayout {
struct Cell { lv_obj_t* object; int width=0; unsigned grow=0; };
inline void place(lv_obj_t* parent,Cell cell) {
  auto* object=cell.object;if(!object)return;
  lv_obj_set_parent(object,parent);lv_obj_set_pos(object,0,0);
  lv_obj_set_width(object,cell.width);lv_obj_set_flex_grow(object,cell.grow);
  if(lv_obj_check_type(object,&lv_label_class)) {
    lv_label_set_long_mode(object,LV_LABEL_LONG_WRAP);
    lv_obj_set_height(object,LV_SIZE_CONTENT);
  }
}
inline lv_obj_t* row(lv_obj_t* parent,std::initializer_list<Cell> cells={}) {
  auto* object=lv_obj_create(parent);lv_obj_remove_style_all(object);
  lv_obj_set_size(object,lv_pct(100),LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(object,48,0);
  lv_obj_set_style_pad_column(object,12,0);
  lv_obj_set_flex_flow(object,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(object,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(object,LV_OBJ_FLAG_SCROLLABLE);
  for(const auto& cell:cells)place(object,cell);
  return object;
}
inline void column(lv_obj_t* object,int padding=0) {
  lv_obj_set_height(object,LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(object,padding,0);
  lv_obj_set_style_pad_row(object,8,0);
  lv_obj_set_flex_flow(object,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(object,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
  lv_obj_remove_flag(object,LV_OBJ_FLAG_SCROLLABLE);
}
inline void card(lv_obj_t* object) {
  column(object,14);
  if(auto* title=lv_obj_get_child(object,0)) {
    lv_obj_set_width(title,lv_pct(100));
    if(lv_obj_check_type(title,&lv_label_class)) {
      lv_label_set_long_mode(title,LV_LABEL_LONG_WRAP);lv_obj_set_height(title,LV_SIZE_CONTENT);
    }
  }
}
inline void panel(lv_obj_t* object) {
  lv_obj_set_style_pad_hor(object,16,0);lv_obj_set_style_pad_top(object,0,0);
  lv_obj_set_style_pad_bottom(object,16,0);lv_obj_set_style_pad_row(object,14,0);
  lv_obj_set_style_pad_column(object,14,0);
  lv_obj_set_flex_flow(object,LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(object,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
  lv_obj_add_flag(object,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(object,LV_DIR_VER);
  lv_obj_set_scrollbar_mode(object,LV_SCROLLBAR_MODE_AUTO);
}
// Two independent content columns: a short card never reserves the height of
// the neighbouring card. Percentage widths remain valid when the dock changes.
inline void columns(lv_obj_t* panel,std::initializer_list<lv_obj_t*> left,
                    std::initializer_list<lv_obj_t*> right) {
  lv_obj_set_flex_flow(panel,LV_FLEX_FLOW_ROW);
  for(const auto& cards:{left,right}) {
    auto* group=lv_obj_create(panel);lv_obj_remove_style_all(group);column(group);
    lv_obj_set_width(group,0);lv_obj_set_flex_grow(group,1);lv_obj_set_style_pad_row(group,14,0);
    for(auto* item:cards){lv_obj_set_parent(item,group);lv_obj_set_pos(item,0,0);lv_obj_set_width(item,lv_pct(100));}
  }
}
inline void slider(lv_obj_t* object) {
  lv_obj_set_height(object,18);lv_obj_set_ext_click_area(object,15);
}
inline void action(lv_obj_t* object) {
  lv_obj_set_height(object,48);
  if(lv_obj_check_type(object,&lv_button_class)) {
    lv_obj_set_height(object,LV_SIZE_CONTENT);lv_obj_set_style_min_height(object,48,0);
    lv_obj_set_style_pad_hor(object,10,0);lv_obj_set_style_pad_ver(object,8,0);
    if(auto* label=lv_obj_get_child(object,0))if(lv_obj_check_type(label,&lv_label_class)) {
      lv_obj_set_width(label,lv_pct(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
      lv_obj_set_height(label,LV_SIZE_CONTENT);lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    }
  }
}
}
