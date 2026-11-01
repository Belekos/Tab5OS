#include "Platform.h"
#include <lvgl.h>
#include "Tab5TextInput.cpp"
#include "Tab5UiStyle.cpp"
#define private public
#include "Tab5MediaList.h"
#undef private
#include "Tab5MediaList.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks=0,activations=0,formatted=0,metadataReads=0;
static unsigned last=0;static bool lastFavorite=false;
static void check(bool value,const char* what){if(!value){fprintf(stderr,"FAIL %s\n",what);exit(1);}++checks;}
static void format(Tab5MediaList::Entry e,String& label,const char*& icon,bool& favorite,bool& allowed,bool& trash){char text[80];snprintf(text,sizeof(text),"Station %04u",e.index);label=text;icon=LV_SYMBOL_WIFI;favorite=e.index%2;allowed=true;trash=false;++formatted;}
static void action(Tab5MediaList::Entry e,bool favorite){++activations;last=e.index;lastFavorite=favorite;}
static void metadata(Tab5MediaList::Entry e,String& name,uint64_t& bytes,int64_t& modified){char text[40];snprintf(text,sizeof(text),"Station %04u",e.index);name=text;bytes=800-e.index;modified=e.index;++metadataReads;}
int main(){
 lv_init();lv_display_create(1280,720);auto* screen=lv_screen_active();Tab5Focus.begin(screen,LV_FONT_DEFAULT);
 uint32_t palette[]={0x101010,0x202020,0x303030,0x777777,0xffffff,0x999999,0x55aaff,0xaaaaaa,0x345678};Tab5UiStyle::configure(palette,12);
 auto* star=Tab5MediaList::starButton(screen,56,48);
 for(unsigned theme=0;theme<2;++theme){
  if(theme){palette[2]=0xeeeedd;palette[4]=0x101010;palette[8]=0xabcabc;Tab5UiStyle::configure(palette,0);Tab5UiStyle::tree(screen);}
  const lv_state_t states[]={LV_STATE_DEFAULT,LV_STATE_PRESSED,LV_STATE_CHECKED,static_cast<lv_state_t>(LV_STATE_PRESSED|LV_STATE_CHECKED)};
  for(auto state:states){lv_obj_remove_state(star,LV_STATE_ANY);lv_obj_add_state(star,state);lv_tick_inc(500);lv_timer_handler();
   check(lv_color_eq(lv_obj_get_style_bg_color(star,LV_PART_MAIN),lv_color_hex(palette[state?8:2])),"favorite fill follows palette for every state and live theme");
   check(lv_color_eq(lv_obj_get_style_text_color(star,LV_PART_MAIN),lv_color_hex(palette[4])),"favorite text follows current palette");
  }
  check(lv_obj_get_style_radius(star,LV_PART_MAIN)==(theme?0:12),"favorite shape follows live theme");
 }
 lv_obj_delete(star);
 auto* list=lv_obj_create(screen);lv_obj_set_size(list,540,500);lv_obj_update_layout(screen);
 {
  Tab5MediaList view(list,LV_FONT_DEFAULT,format,action);
  for(unsigned i=0;i<800;++i)check(view.add(Tab5MediaList::Media,i),"add catalogue reference");
  view.show(0,5);lv_obj_update_layout(screen);
  check(view.count()==800&&lv_obj_get_child_count(list)==25,"800 entries use only 24 rows and one extent");
  check(formatted<=48,"initial render formats only the visible pool");
  check(view.selected_==5&&activations==0,"restoring selection never activates playback");
  view.focus(0);writing::KeyEvent end;end.key=writing::Key::End;check(view.key(end),"End reaches last entry");
  lv_obj_update_layout(screen);check(view.selected_==799&&lv_obj_get_scroll_y(list)>58000,"last entry is accessible beyond old 200 limit");
  writing::KeyEvent enter;enter.key=writing::Key::Enter;check(view.key(enter)&&last==799&&!lastFavorite,"Enter activates actual last station");
  view.focus(0);auto* pressed=view.slots_[0].row;lv_obj_send_event(pressed,LV_EVENT_PRESSED,nullptr);const auto count=activations;
  lv_obj_scroll_to_y(list,30000,LV_ANIM_OFF);lv_obj_update_layout(screen);lv_obj_send_event(pressed,LV_EVENT_CLICKED,nullptr);
  check(activations==count,"scroll cannot turn an old press into a different station");
  view.focus(0);writing::KeyEvent tab;tab.key=writing::Key::Tab;
  for(unsigned i=1;i<=100;++i){check(view.key(tab),"Tab crosses pool boundary");check(view.selected_==i&&Tab5Focus.focused(),"Tab preserves logical focus");}
  writing::KeyEvent right;right.key=writing::Key::Right;check(view.key(right),"Right focuses favorite control");check(view.key(enter)&&last==100&&lastFavorite,"favorite action separate from playback");
  view.focus(3);auto* old=Tab5Focus.focused();lv_obj_send_event(old,LV_EVENT_PRESSED,nullptr);view.reset();view.add(Tab5MediaList::Media,500);view.show(0);lv_obj_send_event(old,LV_EVENT_CLICKED,nullptr);check(last==100,"rebuild rejects old generation click");
  view.reset();for(unsigned i=0;i<800;++i)view.add(Tab5MediaList::Media,i);
  check(view.sort(Tab5FileSort::Order::SizeAscending,metadata),"sort by preloaded file sizes");check(view.entries_[0].index==799&&view.entries_[799].index==0,"ascending size differs from name order");check(metadataReads==800,"metadata read once per item, never in comparator");
  view.show(0);const auto reads=metadataReads;lv_obj_scroll_to_y(list,45000,LV_ANIM_OFF);lv_obj_update_layout(screen);check(metadataReads==reads,"render and scroll do not read filesystem metadata");
  check(view.sort(Tab5FileSort::Order::NameAscending,metadata)&&view.entries_[0].index==0,"name order restored");
  check(view.sort(Tab5FileSort::Order::DateDescending,metadata)&&view.entries_[0].index==799,"date order reversed");
  for(int fail : {0,1,17,799,800}) {
   std::vector<Tab5MediaList::Entry> saved(view.entries_,view.entries_+view.count_);
   const auto resident=HeapCheck::live;HeapCheck::failAfter=fail;
   check(!view.sort(Tab5FileSort::Order::NameAscending,metadata),"failed sort is reported");HeapCheck::failAfter=-1;
   check(HeapCheck::live==resident,"failed sort releases every temporary allocation");
   for(size_t i=0;i<saved.size();++i)check(saved[i].kind==view.entries_[i].kind&&saved[i].index==view.entries_[i].index,"failed sort preserves complete existing order");
  }
  view.show(0);const auto before=activations;lv_obj_send_event(view.slots_[0].row,LV_EVENT_PRESSED,nullptr);lv_obj_send_event(view.slots_[0].row,LV_EVENT_CLICKED,nullptr);check(activations==before+1&&last==799,"mouse executes sorted identity");
  view.release();check(view.count()==0&&view.entries_==nullptr&&view.capacity_==0,"memory reclaim releases catalogue model");
 }
 check(HeapCheck::live==0&&HeapCheck::allocations.empty(),"all explicit PSRAM allocations released");
 check(lv_obj_get_child_count(list)==0,"destruction releases all pooled LVGL children");lv_obj_delete(list);
 printf("PASS %u checks: production LVGL media list, 800 records, pool24, keyboard, mouse, stale clicks, favorite, sort and cleanup; PSRAM peak=%zu bytes.\n",checks,HeapCheck::peak);
}
