#pragma once
#include <stdint.h>
#include <stddef.h>
#define LV_USE_TINY_TTF 1
#define LV_FONT_KERNING_NONE 0
struct lv_font_t {const lv_font_t* fallback=nullptr;unsigned pixels=0;const void* data=nullptr;};
extern const lv_font_t lv_font_montserrat_20,lv_font_montserrat_24,lv_font_montserrat_28,lv_font_montserrat_32;
lv_font_t* lv_tiny_ttf_create_data_ex(const void*,size_t,unsigned,int,unsigned);
void lv_tiny_ttf_destroy(lv_font_t*);
