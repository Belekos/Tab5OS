#pragma once
#include <lvgl.h>
#include <algorithm>
namespace Tab5TopBarLayout {
inline void date(lv_obj_t* label,lv_obj_t* audio,int screenWidth){
 if(!label)return;
 lv_point_t measured{};
 lv_text_get_size(&measured,lv_label_get_text(label),lv_obj_get_style_text_font(label,LV_PART_MAIN),
   lv_obj_get_style_text_letter_space(label,LV_PART_MAIN),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 // Padding protects glyph bearings; reserve actual localized text width.
 const int width=std::max(112,int(measured.x)+8);
 lv_obj_set_style_pad_left(label,4,0);lv_obj_set_style_pad_right(label,4,0);
 lv_obj_set_width(label,width);lv_obj_align(label,LV_ALIGN_RIGHT_MID,-214,0);
 if(audio)lv_obj_set_width(audio,std::max(0,std::min(398,screenWidth-214-width-550-12)));
}
}
