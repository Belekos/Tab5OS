#include <lvgl.h>
#include "Tab5Icons.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static unsigned checks=0;
static void check(bool value,const char* message){++checks;if(!value){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static std::vector<unsigned char> pixels(400*240*3);
static void passive(lv_obj_t* node){
 for(uint32_t i=0;i<lv_obj_get_child_count(node);++i){auto* child=lv_obj_get_child(node,i);
  check(!lv_obj_has_flag(child,LV_OBJ_FLAG_CLICKABLE),"icon child cannot steal clicks");
  check(!lv_obj_has_flag(child,LV_OBJ_FLAG_SCROLLABLE),"icon child cannot scroll");passive(child);
 }
}
int main(int argc,char**argv){
 check(argc==2,"screenshot path");lv_init();auto* display=lv_display_create(400,240);
 lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);
 lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_FULL);
 lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});
 auto* screen=lv_screen_active();lv_obj_set_style_bg_color(screen,lv_color_hex(0x60686a),0);
 lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
 for(unsigned theme=0;theme<2;++theme){const auto ink=lv_color_hex(theme?0xe9eff2:0x1d292f),paper=lv_color_hex(theme?0x24343f:0xf2f6f9);
  for(unsigned icon=0;icon<3;++icon)for(unsigned size=0;size<2;++size){
   auto* button=lv_button_create(screen);lv_obj_remove_style_all(button);lv_obj_set_pos(button,14+icon*130+size*58,16+theme*116);
   lv_obj_set_size(button,size?56:44,size?56:44);lv_obj_set_style_bg_color(button,paper,0);lv_obj_set_style_bg_opa(button,LV_OPA_COVER,0);lv_obj_set_style_radius(button,7,0);lv_obj_set_style_text_color(button,ink,0);
   if(icon==0)tab5AlarmIcon(button,ink,paper);
   else if(icon==1){
    #ifdef TAB5_ICONS_FIXED
    tab5DrawingIcon(button,ink);
    #else
    auto* l=lv_label_create(button);lv_label_set_text(l,LV_SYMBOL_IMAGE);lv_obj_set_style_text_font(l,&lv_font_montserrat_32,0);lv_obj_center(l);lv_obj_remove_flag(l,lv_obj_flag_t(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
    #endif
   }else{auto* l=lv_label_create(button);lv_label_set_text(l,LV_SYMBOL_EYE_OPEN);lv_obj_set_style_text_font(l,&lv_font_montserrat_32,0);lv_obj_center(l);lv_obj_remove_flag(l,lv_obj_flag_t(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));}
   passive(button);lv_obj_update_layout(button);lv_area_t buttonArea;lv_obj_get_coords(button,&buttonArea);
   auto* host=lv_obj_get_child(button,0);lv_area_t iconArea;lv_obj_get_coords(host,&iconArea);
   check(iconArea.x1>=buttonArea.x1&&iconArea.x2<=buttonArea.x2&&iconArea.y1>=buttonArea.y1&&iconArea.y2<=buttonArea.y2,"icon root fits compact and home button");
   lv_point_t center{(buttonArea.x1+buttonArea.x2)/2,(buttonArea.y1+buttonArea.y2)/2};
   check(lv_indev_search_obj(screen,&center)==button,"icon center resolves to parent action");
  }
  auto* label=lv_label_create(screen);lv_label_set_text(label,"Alarm          Drawing           Viewer");lv_obj_set_style_text_font(label,&lv_font_montserrat_14,0);lv_obj_set_style_text_color(label,lv_color_hex(0xffffff),0);lv_obj_set_pos(label,14,82+theme*116);
 }
 lv_obj_update_layout(screen);lv_refr_now(display);FILE* out=fopen(argv[1],"wb");check(out,"open image");fprintf(out,"P6\n400 240\n255\n");
 for(size_t i=0;i<pixels.size();i+=3){unsigned char rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,out);}fclose(out);
 lv_obj_clean(screen);lv_timer_handler();
 printf("{\"passed\":true,\"checks\":%u,\"themes\":2,\"buttonSizes\":[44,56]}\n",checks);
}
