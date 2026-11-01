#include "../../Tab5OS_Arduino_v0.8.18_Ecriture/Tab5ThemeLive.h"
#include "../../Tab5OS_Arduino_v0.8.18_Ecriture/Tab5Appearance.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static void set(lv_obj_t* o,lv_style_prop_t p,uint32_t color,lv_style_selector_t state=0){lv_obj_set_local_style_prop(o,p,{color},state);}
static uint32_t get(lv_obj_t* o,lv_style_prop_t p,lv_style_selector_t state=0){lv_style_value_t v;assert(lv_obj_get_local_style_prop(o,p,&v,state)==LV_STYLE_RES_FOUND);return v.color;}
int main(){
 const uint32_t graphite[]={0x101214,0x191c20,0x24282d,0x353b42,0xf1f3f5,0x9ca3aa,0x3a424a,0xe4e7ea,0x30363d};
 // The green reading theme deliberately has the same background and surface.
 const uint32_t green[]={0xcfe8cc,0xcfe8cc,0xc1dcbe,0x759171,0x19251c,0x40563f,0xa4c6a0,0x254d2b,0xadcfa9};
 auto* screen=new lv_obj_t;auto* hidden=new lv_obj_t;hidden->hidden=true;hidden->radius=12;screen->children.push_back(hidden);
 set(screen,LV_STYLE_BG_COLOR,graphite[0]);set(hidden,LV_STYLE_BG_COLOR,graphite[1]);
 set(hidden,LV_STYLE_BG_COLOR,graphite[6],LV_STATE_PRESSED|LV_STATE_FOCUSED);
 set(hidden,LV_STYLE_TEXT_COLOR,graphite[4]);set(hidden,LV_STYLE_BORDER_COLOR,graphite[3]);
 set(hidden,LV_STYLE_BG_COLOR,graphite[8],LV_PART_SELECTED);
 set(hidden,LV_STYLE_LINE_COLOR,0xf00001); // explicit status/content colour
 for(unsigned i=0;i<200;++i){
  Tab5ThemeLive::apply(screen,graphite,green);
  assert(get(screen,LV_STYLE_BG_COLOR)==green[0]);assert(get(hidden,LV_STYLE_BG_COLOR)==green[1]);
  assert(get(hidden,LV_STYLE_BG_COLOR,LV_STATE_PRESSED|LV_STATE_FOCUSED)==green[6]);
  assert(get(hidden,LV_STYLE_BG_COLOR,LV_PART_SELECTED)==green[8]);
  Tab5ThemeLive::apply(screen,green,graphite);
  assert(get(screen,LV_STYLE_BG_COLOR)==graphite[0]);assert(get(hidden,LV_STYLE_BG_COLOR)==graphite[1]);
  assert(get(hidden,LV_STYLE_LINE_COLOR)==0xf00001);
  assert(screen->events.size()==1&&hidden->events.size()==1);
  assert(hidden->hidden&&hidden->scroll==71&&hidden->cursor==4&&!strcmp(hidden->content,"Unsaved draft"));
 }
 // Controls created after a switch join on the next application.
 Tab5ThemeLive::apply(screen,graphite,green);
 auto* late=new lv_obj_t;late->radius=12;set(late,LV_STYLE_BG_COLOR,green[1]);hidden->children.push_back(late);
 Tab5ThemeLive::apply(screen,green,graphite);assert(get(late,LV_STYLE_BG_COLOR)==graphite[1]);
 // An app can repurpose a style after theme application; it is reclassified.
 set(hidden,LV_STYLE_TEXT_COLOR,graphite[5]);Tab5ThemeLive::apply(screen,graphite,green);
 assert(get(hidden,LV_STYLE_TEXT_COLOR)==green[5]);
 // Empty/nonpalette widgets do not allocate a cache or change custom colours.
 auto* blank=new lv_obj_t;set(blank,LV_STYLE_BG_COLOR,0x123456);Tab5ThemeLive::apply(blank,graphite,green);
 assert(blank->events.empty()&&get(blank,LV_STYLE_BG_COLOR)==0x123456);delete blank;
 delete screen; // registered delete callbacks release caches recursively
 uint32_t ordinary[9],colorful[9];memcpy(ordinary,graphite,sizeof(ordinary));memcpy(colorful,graphite,sizeof(colorful));
 Tab5Appearance::palette(ordinary,false);assert(!memcmp(ordinary,graphite,sizeof(ordinary)));
 Tab5Appearance::palette(colorful,true);
 for(unsigned i:{0U,1U,4U,5U,7U})assert(colorful[i]==graphite[i]);
 for(unsigned i:{2U,3U,6U,8U})assert(colorful[i]!=graphite[i]);
 puts("PASS: live theme 200 round trips, duplicate colours, hidden pages, combined states, selection part, late controls, external style changes, content/scroll/caret preservation, deletion, optional accent isolation.");
}
