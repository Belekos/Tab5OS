#pragma once
#include <lvgl.h>
#include <stdint.h>
namespace Tab5DocumentText {
inline uint16_t pixels(uint8_t index){const uint16_t sizes[]={20,24,28,32};return sizes[index>3?3:index];}
// TinyTTF keeps the same font pointer when its size changes. Force the label's
// measurement cache to refresh even if setting that pointer is a LVGL no-op.
inline void apply(lv_obj_t* object,const lv_font_t* font){
 if(!object||!lv_obj_is_valid(object))return;
 lv_obj_set_style_text_font(object,font,LV_PART_MAIN);
 lv_obj_refresh_style(object,LV_PART_MAIN,LV_STYLE_TEXT_FONT);
}
}
