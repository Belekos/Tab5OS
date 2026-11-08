#include "Tab5UiStyle.h"
#include "Tab5TextInput.h"
#include "Tab5ThemeLive.h"
#include <algorithm>
namespace Tab5UiStyle {
namespace {
uint32_t palette[]={0xf4f4f0,0xffffff,0xe8e8e4,0x777777,0x161616,0x666666,0x365b78,0x161616,0xcbdce8};
uint8_t corners=7;
uint32_t switchOffColor=0xe8e8e4,switchOnColor=0x365b78;
uint32_t switchKnobOffColor=0x161616,switchKnobOnColor=0xffffff;
lv_color_t c(unsigned i){return lv_color_hex(palette[i]);}
void cardMarker(lv_event_t*){}
void buttonMarker(lv_event_t*){}
void selectionMarker(lv_event_t*){}
struct DropdownHover {
 lv_obj_t* owner=nullptr;
 lv_obj_t* list=nullptr;
 int row=-1;
 bool selectedHighlight=true;
};
DropdownHover dropdownHover;
lv_obj_t* pointerObject=nullptr;
bool pointerActive=false;
int pointerX=0,pointerY=0;
uint8_t pointerButtons=0;
void dropdownEvent(lv_event_t* e);
void dropdownListEvent(lv_event_t* e);
bool watches(lv_obj_t* o,lv_event_cb_t callback){for(uint32_t i=0;i<lv_obj_get_event_count(o);++i)if(lv_event_dsc_get_cb(lv_obj_get_event_dsc(o,i))==callback)return true;return false;}
void pointerDeleted(lv_event_t* e){if(lv_event_get_target_obj(e)==pointerObject)pointerObject=nullptr;}
void pointerObjectSet(lv_obj_t* o){
 if(o==pointerObject){if(o&&pointerActive)lv_obj_add_state(o,LV_STATE_HOVERED);return;}
 if(pointerObject){lv_obj_remove_state(pointerObject,LV_STATE_HOVERED);lv_obj_remove_event_cb_with_user_data(pointerObject,pointerDeleted,nullptr);}
 pointerObject=nullptr;
 if(o&&lv_obj_add_event_cb(o,pointerDeleted,LV_EVENT_DELETE,nullptr)){pointerObject=o;if(pointerActive)lv_obj_add_state(o,LV_STATE_HOVERED);}
}
void selectedHighlight(lv_obj_t* owner,bool value){
 if(lv_dropdown_get_selected_highlight(owner)!=value)lv_dropdown_set_selected_highlight(owner,value);
}
void dropdownHoverEnd(bool restore){
 if(dropdownHover.owner&&restore)selectedHighlight(dropdownHover.owner,dropdownHover.selectedHighlight);
 dropdownHover={};
}
int dropdownRow(int x,int y){
 auto* list=dropdownHover.list;
 if(!list||!lv_obj_is_visible(list)||pointerButtons)return -1;
 lv_area_t area;lv_obj_get_coords(list,&area);
 if(x<area.x1||x>area.x2||y<area.y1||y>area.y2)return -1;
 auto* label=lv_obj_get_child(list,0);if(!label)return -1;
 lv_obj_get_coords(label,&area);
 const int spacing=lv_obj_get_style_text_line_space(label,LV_PART_MAIN);
 const int line=lv_font_get_line_height(lv_obj_get_style_text_font(label,LV_PART_MAIN))+spacing;
 const int relative=y-area.y1+spacing/2;
 if(relative<0||line<=0)return -1;
 const int row=relative/line;
 return row<int(lv_dropdown_get_option_count(dropdownHover.owner))?row:-1;
}
void dropdownRowSet(int row){
 if(row==dropdownHover.row)return;
 dropdownHover.row=row;
 if(dropdownHover.list)lv_obj_invalidate(dropdownHover.list);
}
bool intersect(lv_area_t& out,const lv_area_t& a,const lv_area_t& b){
 out={std::max(a.x1,b.x1),std::max(a.y1,b.y1),std::min(a.x2,b.x2),std::min(a.y2,b.y2)};
 return out.x1<=out.x2&&out.y1<=out.y2;
}
void dropdownDraw(lv_event_t* e){
 auto* list=lv_event_get_current_target_obj(e);
 if(!pointerActive||dropdownHover.list!=list||dropdownHover.row<0||pointerButtons)return;
 auto* label=lv_obj_get_child(list,0);if(!label)return;
 auto* layer=lv_event_get_layer(e);lv_area_t labelArea,listArea,rowArea,clip;
 lv_obj_get_coords(label,&labelArea);lv_obj_get_coords(list,&listArea);
 const auto* font=lv_obj_get_style_text_font(label,LV_PART_MAIN);
 const int spacing=lv_obj_get_style_text_line_space(label,LV_PART_MAIN);
 const int height=lv_font_get_line_height(font);
 rowArea={listArea.x1,labelArea.y1+dropdownHover.row*(height+spacing)-spacing/2,
          listArea.x2,labelArea.y1+(dropdownHover.row+1)*(height+spacing)-spacing/2-1};
 if(!intersect(clip,rowArea,listArea)||!intersect(clip,clip,layer->_clip_area))return;
 // LVGL snapshots this public layer clip into each queued draw task. Restore
 // it immediately; no widget selection or private dropdown field is changed.
 const lv_area_t previousClip=layer->_clip_area;layer->_clip_area=clip;
 lv_draw_rect_dsc_t rect;lv_draw_rect_dsc_init(&rect);lv_obj_init_draw_rect_dsc(list,LV_PART_SELECTED,&rect);
 lv_draw_rect(layer,&rect,&rowArea);
 lv_draw_label_dsc_t text;lv_draw_label_dsc_init(&text);lv_obj_init_draw_label_dsc(label,LV_PART_MAIN,&text);
 text.color=lv_obj_get_style_text_color(list,LV_PART_SELECTED);text.text=lv_label_get_text(label);text.flag=LV_TEXT_FLAG_EXPAND;
 lv_draw_label(layer,&text,&labelArea);
 layer->_clip_area=previousClip;
}
void dropdownListEvent(lv_event_t* e){
 const auto code=lv_event_get_code(e);auto* list=lv_event_get_current_target_obj(e);
 if(code==LV_EVENT_DRAW_POST)dropdownDraw(e);
 else if(code==LV_EVENT_DELETE&&dropdownHover.list==list)dropdownHoverEnd(false);
 else if(code==LV_EVENT_SCROLL&&dropdownHover.list==list&&pointerActive)dropdownRowSet(dropdownRow(pointerX,pointerY));
}
void dropdownEvent(lv_event_t* e){
 auto* owner=lv_event_get_current_target_obj(e);const auto code=lv_event_get_code(e);
 if(code==LV_EVENT_READY){
  control(owner);
  if(dropdownHover.owner!=owner){dropdownHoverEnd(true);dropdownHover.owner=owner;dropdownHover.list=lv_dropdown_get_list(owner);dropdownHover.selectedHighlight=lv_dropdown_get_selected_highlight(owner);}
  dropdownRowSet(-1);
  if(pointerActive)selectedHighlight(owner,false);
 }else if(code==LV_EVENT_CANCEL&&dropdownHover.owner==owner)dropdownHoverEnd(true);
 else if(code==LV_EVENT_DELETE&&dropdownHover.owner==owner)dropdownHoverEnd(false);
}
void touchEvent(lv_event_t* e){
 const auto code=lv_event_get_code(e);
 if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_HOVER_OVER)return;
 if(code==LV_EVENT_PRESSED)pointerLeave();
 // The indev supplies the live event target; no touch target is retained.
 if(auto* object=static_cast<lv_obj_t*>(lv_event_get_param(e)))lv_obj_remove_state(object,LV_STATE_HOVERED);
}
void focus(lv_obj_t* o){
 lv_obj_set_style_outline_color(o,c(6),LV_STATE_FOCUS_KEY);
 lv_obj_set_style_outline_width(o,2,LV_STATE_FOCUS_KEY);
 lv_obj_set_style_outline_pad(o,2,LV_STATE_FOCUS_KEY);
}
}
void configure(const uint32_t* colors,uint8_t value,uint32_t off,uint32_t on,uint32_t knobOff,uint32_t knobOn){
 if(colors)std::copy(colors,colors+9,palette);corners=std::min<uint8_t>(value,24);
 switchOffColor=off<=0xffffff?off:palette[2];switchOnColor=on<=0xffffff?on:palette[6];
 switchKnobOffColor=knobOff<=0xffffff?knobOff:palette[4];switchKnobOnColor=knobOn<=0xffffff?knobOn:palette[1];
}
uint32_t color(unsigned role){return role==11?switchOffColor:role==12?switchOnColor:role==13?switchKnobOffColor:role==14?switchKnobOnColor:palette[role<9?role:4];}
uint8_t radius(){return corners;}
void button(lv_obj_t* o){
 if(!o||!lv_obj_is_valid(o))return;
 if(!watches(o,buttonMarker))lv_obj_add_event_cb(o,buttonMarker,LV_EVENT_DELETE,nullptr);
 lv_obj_set_style_bg_color(o,c(2),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
 lv_obj_set_style_text_color(o,c(4),0);lv_obj_set_style_border_color(o,c(3),0);
 lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_shadow_width(o,0,0);
 lv_obj_set_style_radius(o,corners,0);lv_obj_set_style_outline_width(o,0,0);
 selection(o);
} 
void selection(lv_obj_t* o){
 if(!o||!lv_obj_is_valid(o))return;
 if(!watches(o,selectionMarker))lv_obj_add_event_cb(o,selectionMarker,LV_EVENT_DELETE,nullptr);
 const lv_state_t states[]={LV_STATE_HOVERED,LV_STATE_PRESSED,LV_STATE_CHECKED,static_cast<lv_state_t>(LV_STATE_CHECKED|LV_STATE_PRESSED)};
 for(auto state:states){
  lv_obj_set_style_bg_color(o,c(8),state);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,state);lv_obj_set_style_text_color(o,c(4),state);
  lv_obj_set_style_color_filter_opa(o,LV_OPA_TRANSP,state);
 }
 lv_obj_set_style_opa(o,LV_OPA_50,LV_STATE_DISABLED);focus(o);
}
void switchColors(lv_obj_t* o,uint32_t off,uint32_t on,uint32_t knobOff,uint32_t knobOn){
 if(!o||!lv_obj_is_valid(o))return;
 for(auto part:{LV_PART_MAIN,LV_PART_INDICATOR}){
  lv_obj_set_style_bg_color(o,lv_color_hex(off),part);
  lv_obj_set_style_bg_color(o,lv_color_hex(on),part|LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(o,LV_OPA_COVER,part);
  lv_obj_set_style_bg_opa(o,LV_OPA_COVER,part|LV_STATE_CHECKED);
 }
 lv_obj_set_style_bg_color(o,lv_color_hex(knobOff),LV_PART_KNOB);
 lv_obj_set_style_bg_color(o,lv_color_hex(knobOn),LV_PART_KNOB|LV_STATE_CHECKED);
 lv_obj_set_style_bg_opa(o,LV_OPA_COVER,LV_PART_KNOB);
 // The thumb must remain outlined even when a custom theme gives it the
 // same fill as the track. Choose a dark/light edge against its own fill.
 auto edge=[](uint32_t value){return (((value>>16)&255)*299+((value>>8)&255)*587+(value&255)*114)>=128000?0x101010u:0xf4f4f4u;};
 lv_obj_set_style_border_width(o,2,LV_PART_KNOB);
 lv_obj_set_style_border_width(o,2,LV_PART_KNOB|LV_STATE_CHECKED);
 lv_obj_set_style_border_opa(o,LV_OPA_COVER,LV_PART_KNOB);
 lv_obj_set_style_border_opa(o,LV_OPA_COVER,LV_PART_KNOB|LV_STATE_CHECKED);
 lv_obj_set_style_border_color(o,lv_color_hex(edge(knobOff)),LV_PART_KNOB);
 lv_obj_set_style_border_color(o,lv_color_hex(edge(knobOn)),LV_PART_KNOB|LV_STATE_CHECKED);
}
void control(lv_obj_t* o){
 if(!o||!lv_obj_is_valid(o)||Tab5ThemeLive::preserved(o))return;
 if(lv_obj_check_type(o,&lv_switch_class)){
  switchColors(o,switchOffColor,switchOnColor,switchKnobOffColor,switchKnobOnColor);
  lv_obj_set_style_border_width(o,1,LV_PART_MAIN);lv_obj_set_style_border_color(o,c(3),LV_PART_MAIN);
  lv_obj_set_style_radius(o,corners,LV_PART_MAIN);lv_obj_set_style_radius(o,corners,LV_PART_INDICATOR);lv_obj_set_style_radius(o,corners,LV_PART_KNOB);focus(o);
 }else if(lv_obj_check_type(o,&lv_slider_class)||lv_obj_check_type(o,&lv_bar_class)){
  lv_obj_set_style_bg_color(o,c(2),LV_PART_MAIN);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,LV_PART_MAIN);
  lv_obj_set_style_bg_color(o,c(6),LV_PART_INDICATOR);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,LV_PART_INDICATOR);
  lv_obj_set_style_radius(o,corners,LV_PART_MAIN);lv_obj_set_style_radius(o,corners,LV_PART_INDICATOR);
  if(lv_obj_check_type(o,&lv_slider_class)){lv_obj_set_style_bg_color(o,c(4),LV_PART_KNOB);lv_obj_set_style_border_color(o,c(1),LV_PART_KNOB);lv_obj_set_style_border_width(o,2,LV_PART_KNOB);lv_obj_set_style_radius(o,corners,LV_PART_KNOB);focus(o);}
 }else if(lv_obj_check_type(o,&lv_checkbox_class)){
  lv_obj_set_style_text_color(o,c(4),LV_PART_MAIN);
  lv_obj_set_style_bg_color(o,c(1),LV_PART_INDICATOR);lv_obj_set_style_border_color(o,c(3),LV_PART_INDICATOR);
  for(auto state:{LV_STATE_CHECKED,LV_STATE_PRESSED,lv_state_t(LV_STATE_CHECKED|LV_STATE_PRESSED)}){
   lv_obj_set_style_bg_color(o,c(8),LV_PART_INDICATOR|state);lv_obj_set_style_text_color(o,c(4),LV_PART_INDICATOR|state);lv_obj_set_style_border_color(o,c(3),LV_PART_INDICATOR|state);lv_obj_set_style_color_filter_opa(o,LV_OPA_TRANSP,LV_PART_INDICATOR|state);
  }
  lv_obj_set_style_radius(o,std::min<uint8_t>(corners,8),LV_PART_INDICATOR);focus(o);
 }else if(lv_obj_check_type(o,&lv_dropdown_class)){
  if(!watches(o,dropdownEvent))lv_obj_add_event_cb(o,dropdownEvent,LV_EVENT_ALL,nullptr);
  lv_obj_set_style_bg_color(o,c(1),LV_PART_MAIN);lv_obj_set_style_text_color(o,c(4),LV_PART_MAIN);lv_obj_set_style_border_color(o,c(3),LV_PART_MAIN);lv_obj_set_style_border_width(o,1,LV_PART_MAIN);lv_obj_set_style_radius(o,corners,LV_PART_MAIN);focus(o);
  auto* list=lv_dropdown_get_list(o);if(list){if(!watches(list,dropdownListEvent))lv_obj_add_event_cb(list,dropdownListEvent,LV_EVENT_ALL,nullptr);lv_obj_set_style_bg_color(list,c(1),0);lv_obj_set_style_text_color(list,c(4),0);lv_obj_set_style_border_color(list,c(3),0);lv_obj_set_style_radius(list,corners,0);for(auto state:{LV_STATE_DEFAULT,LV_STATE_CHECKED,LV_STATE_PRESSED,lv_state_t(LV_STATE_CHECKED|LV_STATE_PRESSED)}){lv_obj_set_style_bg_color(list,c(8),LV_PART_SELECTED|state);lv_obj_set_style_bg_opa(list,LV_OPA_COVER,LV_PART_SELECTED|state);lv_obj_set_style_text_color(list,c(4),LV_PART_SELECTED|state);lv_obj_set_style_color_filter_opa(list,LV_OPA_TRANSP,LV_PART_SELECTED|state);}lv_obj_set_style_text_font(list,lv_obj_get_style_text_font(o,LV_PART_MAIN),0);}
 }else if(lv_obj_check_type(o,&lv_textarea_class)){
  Tab5TextInput::track(o);
  lv_obj_set_style_radius(o,corners,LV_PART_MAIN);lv_obj_set_style_bg_color(o,c(1),0);lv_obj_set_style_text_color(o,c(4),0);lv_obj_set_style_border_color(o,c(3),0);lv_obj_set_style_bg_color(o,c(8),LV_PART_SELECTED);lv_obj_set_style_text_color(o,c(4),LV_PART_SELECTED);lv_obj_set_style_bg_color(o,c(7),LV_PART_CURSOR);focus(o);
 }else if(lv_obj_check_type(o,&lv_button_class)){
  if(watches(o,buttonMarker))button(o);else if(watches(o,selectionMarker))selection(o);
  lv_style_value_t side{};bool tab=lv_obj_get_local_style_prop(o,LV_STYLE_BORDER_SIDE,&side,LV_PART_MAIN|LV_STATE_CHECKED)==LV_STYLE_RES_FOUND&&side.num==LV_BORDER_SIDE_BOTTOM;
  if(!tab)lv_obj_set_style_radius(o,corners,0);focus(o);
 }else if(watches(o,cardMarker)||((lv_obj_get_width(o)>100||lv_obj_get_style_width(o,LV_PART_MAIN)>100)&&(lv_obj_get_height(o)>60||lv_obj_get_style_height(o,LV_PART_MAIN)>60)&&lv_obj_get_style_radius(o,LV_PART_MAIN)>0&&!lv_obj_check_type(o,&lv_label_class))){
  if(!watches(o,cardMarker))lv_obj_add_event_cb(o,cardMarker,LV_EVENT_DELETE,nullptr);lv_obj_set_style_radius(o,corners,0);
 }
}
void tree(lv_obj_t* root){if(!root||!lv_obj_is_valid(root)||Tab5ThemeLive::preserved(root))return;control(root);for(unsigned i=0;i<lv_obj_get_child_count(root);++i)tree(lv_obj_get_child(root,i));}
void pointer(lv_indev_t* input,int x,int y,uint8_t buttons){
 pointerActive=true;pointerX=x;pointerY=y;pointerButtons=buttons;
 lv_point_t point{x,y};auto* display=lv_indev_get_display(input);if(!display)return;
 // Use the same native hit test and layer order as the pointer device. This
 // respects clipping, hidden controls and an overlay above a menu.
 auto* hit=lv_indev_search_obj(lv_display_get_layer_sys(display),&point);
 if(!hit)hit=lv_indev_search_obj(lv_display_get_layer_top(display),&point);
 if(!hit)hit=lv_indev_search_obj(lv_display_get_screen_active(display),&point);
 if(!hit)hit=lv_indev_search_obj(lv_display_get_layer_bottom(display),&point);
 pointerObjectSet(hit);
 if(dropdownHover.owner){
  selectedHighlight(dropdownHover.owner,false);
  dropdownRowSet(hit==dropdownHover.list?dropdownRow(x,y):-1);
 }
}
void keyboardMode(){
 pointerActive=false;
 if(pointerObject)lv_obj_remove_state(pointerObject,LV_STATE_HOVERED);
 dropdownRowSet(-1);
 if(dropdownHover.owner)selectedHighlight(dropdownHover.owner,true);
}
void touchInput(lv_indev_t* input){
 if(!input)return;
 for(uint32_t i=0;i<lv_indev_get_event_count(input);++i)if(lv_event_dsc_get_cb(lv_indev_get_event_dsc(input,i))==touchEvent)return;
 lv_indev_add_event_cb(input,touchEvent,LV_EVENT_ALL,nullptr);
}
void pointerLeave(){
 pointerActive=false;pointerButtons=0;pointerObjectSet(nullptr);dropdownRowSet(-1);
 if(dropdownHover.owner)selectedHighlight(dropdownHover.owner,dropdownHover.selectedHighlight);
}

}
