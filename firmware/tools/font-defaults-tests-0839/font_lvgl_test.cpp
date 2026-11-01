#include "Tab5FontDefaults.h"
#include <lvgl.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <cstdlib>
static unsigned checks=0;
static void check(bool b){++checks;if(!b){std::cerr<<"failed at "<<checks<<"\n";std::abort();}}
static std::vector<uint8_t> read(const char* p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
 check(argc==3);auto regular=read(argv[1]),italic=read(argv[2]);
 check(Tab5FontDefaults::validate(regular.data(),regular.size()));check(Tab5FontDefaults::validate(italic.data(),italic.size()));
 lv_init();
 for(unsigned pixels:{20u,24u,28u,32u,34u}){
  auto* fallback=lv_tiny_ttf_create_data_ex(regular.data(),regular.size(),pixels,LV_FONT_KERNING_NORMAL,96);
  auto* chosen=lv_tiny_ttf_create_data_ex(italic.data(),italic.size(),pixels,LV_FONT_KERNING_NORMAL,96);
  check(fallback&&chosen);chosen->fallback=fallback;
  for(uint32_t point:{uint32_t('A'),uint32_t('W'),uint32_t(0x00e9),uint32_t(0x03a9),uint32_t(0x0416),uint32_t(0x0627),uint32_t(0xfe8d)}){
   lv_font_glyph_dsc_t d{};check(lv_font_get_glyph_dsc(chosen,&d,point,0));check(d.resolved_font==chosen||d.resolved_font==fallback);check(d.box_w<pixels*4&&d.box_h<pixels*4);
  }
  lv_font_glyph_dsc_t arabic{};check(lv_font_get_glyph_dsc(chosen,&arabic,0x0627,0));check(arabic.resolved_font==fallback);
  // Document zoom must resize both the primary face and its standard fallback.
  lv_tiny_ttf_set_size(chosen,pixels+4);lv_tiny_ttf_set_size(fallback,pixels+4);
  check(chosen->line_height==fallback->line_height);
  chosen->fallback=nullptr;lv_tiny_ttf_destroy(chosen);lv_tiny_ttf_destroy(fallback);
 }
 lv_deinit();std::cout<<"PASS "<<checks<<" actual LVGL font/fallback/zoom checks\n";
}
