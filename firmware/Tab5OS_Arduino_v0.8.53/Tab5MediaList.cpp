#include "Tab5MediaList.h"
#include "Tab5UiStyle.h"
#include "Tab5Focus.h"
#include "Tab5Localization.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <esp_heap_caps.h>

namespace {
lv_color_t color(unsigned role) { return lv_color_hex(Tab5UiStyle::color(role)); }
void drawStar(lv_event_t* event) {
  auto* button = lv_event_get_target_obj(event);
  if(lv_obj_has_state(button,LV_STATE_USER_1))return;
  lv_area_t bounds; lv_obj_get_coords(button, &bounds);
  const int x = (bounds.x1 + bounds.x2) / 2, y = (bounds.y1 + bounds.y2) / 2;
  static const int8_t points[10][2] = {
    {0,-15},{4,-5},{15,-5},{7,2},{10,14},{0,7},{-10,14},{-7,2},{-15,-5},{-4,-5}
  };
  auto* layer = lv_event_get_layer(event);
  const auto ink = color(4);
  const bool filled = lv_obj_has_state(button, LV_STATE_CHECKED);
  for (unsigned i = 0; i < 10; ++i) {
    const unsigned j = (i + 1) % 10;
    if (filled) {
      lv_draw_triangle_dsc_t triangle; lv_draw_triangle_dsc_init(&triangle);
      triangle.color = ink;
      triangle.p[0] = {lv_value_precise_t(x),lv_value_precise_t(y)};
      triangle.p[1] = {lv_value_precise_t(x+points[i][0]),lv_value_precise_t(y+points[i][1])};
      triangle.p[2] = {lv_value_precise_t(x+points[j][0]),lv_value_precise_t(y+points[j][1])};
      lv_draw_triangle(layer,&triangle);
    }
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
    line.color = ink; line.width = 2;
    line.p1 = {lv_value_precise_t(x+points[i][0]),lv_value_precise_t(y+points[i][1])};
    line.p2 = {lv_value_precise_t(x+points[j][0]),lv_value_precise_t(y+points[j][1])};
    lv_draw_line(layer,&line);
  }
}
}
lv_obj_t* Tab5MediaList::starButton(lv_obj_t* parent, int width, int height) {
  auto* button=lv_button_create(parent);lv_obj_set_size(button,width,height);
  lv_obj_remove_flag(button,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_EVENT_BUBBLE));
  Tab5UiStyle::button(button);
  lv_obj_add_event_cb(button,drawStar,LV_EVENT_DRAW_MAIN,nullptr);
  Tab5Focus.describe(button,Tab5Locale.text("Favori : ajouter ou retirer"));
  return button;
}
Tab5MediaList::Tab5MediaList(lv_obj_t* list,const lv_font_t* font,Format format,Action action)
 :list_(list),font_(font),format_(format),action_(action) {
  lv_obj_set_layout(list_,LV_LAYOUT_NONE);lv_obj_set_scroll_dir(list_,LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list_,LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_pad_all(list_,0,0);lv_obj_set_style_pad_right(list_,18,0);
  lv_obj_add_flag(list_,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(list_,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
  extent_=lv_obj_create(list_);lv_obj_remove_style_all(extent_);
  lv_obj_set_size(extent_,1,1);lv_obj_remove_flag(extent_,LV_OBJ_FLAG_CLICKABLE);
  for(auto& slot:slots_) {
    slot.owner=this;slot.row=lv_button_create(list_);lv_obj_set_size(slot.row,lv_pct(100),Height-4);
    lv_obj_remove_flag(slot.row,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(slot.row,0,0);
    slot.icon=lv_label_create(slot.row);lv_obj_set_style_text_font(slot.icon,&lv_font_montserrat_24,0);
    lv_obj_align(slot.icon,LV_ALIGN_LEFT_MID,10,0);
    slot.label=lv_label_create(slot.row);lv_label_set_long_mode(slot.label,LV_LABEL_LONG_DOT);
    lv_obj_align(slot.label,LV_ALIGN_LEFT_MID,48,0);
    slot.favorite=starButton(slot.row,46,44);lv_obj_align(slot.favorite,LV_ALIGN_RIGHT_MID,-8,0);
    slot.trash=lv_label_create(slot.favorite);lv_label_set_text(slot.trash,LV_SYMBOL_TRASH);
    lv_obj_set_style_text_font(slot.trash,&lv_font_montserrat_24,0);lv_obj_center(slot.trash);
    lv_obj_add_flag(slot.trash,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(slot.row,rowEvent,LV_EVENT_PRESSED,&slot);
    lv_obj_add_event_cb(slot.row,rowEvent,LV_EVENT_CLICKED,&slot);
    lv_obj_add_event_cb(slot.favorite,rowEvent,LV_EVENT_PRESSED,&slot);
    lv_obj_add_event_cb(slot.favorite,rowEvent,LV_EVENT_CLICKED,&slot);
    lv_obj_add_flag(slot.row,LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_add_event_cb(list_,listEvent,LV_EVENT_SCROLL,this);
  lv_obj_add_event_cb(list_,listEvent,LV_EVENT_SIZE_CHANGED,this);
  appearance(font_);
}
Tab5MediaList::~Tab5MediaList() {
  heap_caps_free(entries_);heap_caps_free(pending_);
  if(list_&&lv_obj_is_valid(list_)) {
    if(Tab5Focus.contains(list_,Tab5Focus.focused()))Tab5Focus.clear();
    lv_obj_remove_event_cb_with_user_data(list_,listEvent,this);lv_obj_clean(list_);
  }
}
void Tab5MediaList::appearance(const lv_font_t* font) {
  bool changed=!appearanceSet_ || (font && font!=font_) || radius_!=Tab5UiStyle::radius();
  for(unsigned i=0;i<9;++i)if(colors_[i]!=Tab5UiStyle::color(i))changed=true;
  if(!changed)return;
  appearanceSet_=true;radius_=Tab5UiStyle::radius();
  for(unsigned i=0;i<9;++i)colors_[i]=Tab5UiStyle::color(i);
  if(font)font_=font;
  lv_obj_set_style_bg_color(list_,color(1),0);lv_obj_set_style_text_color(list_,color(4),0);
  for(auto& slot:slots_) {
    lv_obj_set_style_bg_color(slot.row,color(1),0);lv_obj_set_style_text_color(slot.row,color(4),0);
    lv_obj_set_style_bg_color(slot.row,color(8),LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(slot.row,color(8),LV_STATE_PRESSED);
    lv_obj_set_style_text_font(slot.label,font_,0);Tab5UiStyle::control(slot.row);
    Tab5UiStyle::control(slot.favorite);
  }
}
void Tab5MediaList::reset() {
  rebuilding_=true;pendingCount_=0;
  // Keep pixels, scroll geometry and focus until the replacement is complete.
  // A press begun before rebuilding must never activate a different entry.
  for(auto& slot:slots_)slot.pressed=SIZE_MAX;
}
void Tab5MediaList::cancel() {
  pendingCount_=0;rebuilding_=false;
}
bool Tab5MediaList::add(Kind kind,uint32_t index) {
  auto*& entries=rebuilding_?pending_:entries_;
  auto& count=rebuilding_?pendingCount_:count_;
  auto& capacity=rebuilding_?pendingCapacity_:capacity_;
  // Catalogue limits are smaller; this protects scroll arithmetic on corrupt input.
  if(count>=8192)return false;
  if(count==capacity) {
    const size_t nextCapacity=std::min<size_t>(8192,capacity?capacity*2:64);
    auto* next=static_cast<Entry*>(heap_caps_realloc(entries,nextCapacity*sizeof(Entry),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!next)return false;
    entries=next;capacity=nextCapacity;
  }
  entries[count++]={kind,index};return true;
}
bool Tab5MediaList::sort(Tab5FileSort::Order order,Metadata metadata) {
  auto* entries=rebuilding_?pending_:entries_;
  const auto count=rebuilding_?pendingCount_:count_;
  if(count<2)return true;
  // Catalogue data must not compete with sockets and DMA for internal SRAM.
  struct Key { Entry entry;union { char* name;size_t nameOffset; };bool directory;uint64_t bytes;int64_t modified; };
  auto* keys=static_cast<Key*>(heap_caps_calloc(count,sizeof(Key),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!keys)return false;
  bool ok=true;char* names=nullptr;size_t used=0,capacity=0;
  for(size_t i=0;i<count;++i) {
    auto& key=keys[i];key.entry=entries[i];key.directory=key.entry.kind==Folder;
    String name;metadata(key.entry,name,key.bytes,key.modified);
    if(name.length()>SIZE_MAX-used-1){ok=false;break;}
    const size_t required=used+name.length()+1;
    if(required>capacity){
      const size_t grown=capacity<=SIZE_MAX/2?capacity*2:required;
      const size_t nextCapacity=std::max(required,std::max<size_t>(256,grown));
      auto* next=static_cast<char*>(heap_caps_realloc(names,nextCapacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
      if(!next){ok=false;break;}
      names=next;capacity=nextCapacity;
    }
    key.nameOffset=used;std::memcpy(names+used,name.c_str(),name.length()+1);used=required;
  }
  if(ok) {
    for(size_t i=0;i<count;++i)keys[i].name=names+keys[i].nameOffset;
    auto rank=[](Kind kind){switch(kind){case Parent:return 0;case Folder:return 1;case Media:return 2;case Message:return 3;default:return 4;}};
    std::sort(keys,keys+count,[&](const Key& a,const Key& b){
      const int ar=rank(a.entry.kind),br=rank(b.entry.kind);
      if(ar!=br)return ar<br;
      return Tab5FileSort::less(a,b,order);
    });
    for(size_t i=0;i<count;++i)entries[i]=keys[i].entry;
    if(!rebuilding_)++generation_;
  }
  heap_caps_free(names);heap_caps_free(keys);return ok;
}
void Tab5MediaList::release() {
  rebuilding_=false;count_=pendingCount_=0;selected_=SIZE_MAX;++generation_;
  heap_caps_free(entries_);heap_caps_free(pending_);entries_=pending_=nullptr;
  capacity_=pendingCapacity_=0;
  if(Tab5Focus.contains(list_,Tab5Focus.focused()))Tab5Focus.clear();
  for(auto& slot:slots_){slot.index=slot.pressed=SIZE_MAX;lv_obj_add_flag(slot.row,LV_OBJ_FLAG_HIDDEN);}
  lv_obj_set_y(extent_,0);
}
void Tab5MediaList::show(int32_t scroll,uint32_t selected) {
  if(rebuilding_) {
    std::swap(entries_,pending_);std::swap(capacity_,pendingCapacity_);
    count_=pendingCount_;pendingCount_=0;rebuilding_=false;++generation_;
  }
  selected_=SIZE_MAX;
  for(size_t i=0;i<count_;++i)
    if(entries_[i].kind==Media&&entries_[i].index==selected){selected_=i;break;}
  lv_obj_set_y(extent_,!count_?0:int32_t(count_*Height));
  lv_obj_update_layout(list_);
  lv_obj_scroll_to_y(list_,std::max<int32_t>(0,scroll),LV_ANIM_OFF);render();
}
void Tab5MediaList::select(uint32_t index) {
  size_t wanted=SIZE_MAX;
  for(size_t i=0;i<count_;++i)if(entries_[i].kind==Media&&entries_[i].index==index){wanted=i;break;}
  if(wanted!=selected_){selected_=wanted;render();}
}
void Tab5MediaList::render() {
  if(rendering_||rebuilding_)return;rendering_=true;
  const int32_t top=std::max<int32_t>(0,lv_obj_get_scroll_y(list_));
  const size_t first=std::min<size_t>(count_>Pool?count_-Pool:0,size_t(top/Height));
  const int32_t width=lv_obj_get_content_width(list_);
  for(unsigned i=0;i<Pool;++i) {
    auto& slot=slots_[i];const size_t index=first+i;
    if(index>=count_) {
      if(Tab5Focus.contains(slot.row,Tab5Focus.focused()))Tab5Focus.clear();
      slot.index=SIZE_MAX;lv_obj_add_flag(slot.row,LV_OBJ_FLAG_HIDDEN);continue;
    }
    if(slot.index!=index || slot.formattedGeneration!=generation_) {
      String label;const char* icon="";bool favorite=false,canFavorite=false,trash=false;
      format_(entries_[index],label,icon,favorite,canFavorite,trash);
      const bool labelChanged=std::strcmp(lv_label_get_text(slot.label),label.c_str())!=0;
      const bool iconChanged=std::strcmp(lv_label_get_text(slot.icon),icon)!=0;
      if((slot.index!=index || labelChanged || iconChanged) &&
         Tab5Focus.contains(slot.row,Tab5Focus.focused()))Tab5Focus.clear();
      slot.index=index;slot.pressed=SIZE_MAX;slot.formattedGeneration=generation_;
      if(labelChanged)lv_label_set_text(slot.label,label.c_str());
      if(iconChanged)lv_label_set_text(slot.icon,icon);
      Tab5Focus.describe(slot.row,label.c_str());
      if(canFavorite)lv_obj_remove_flag(slot.favorite,LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(slot.favorite,LV_OBJ_FLAG_HIDDEN);
      if(favorite)lv_obj_add_state(slot.favorite,LV_STATE_CHECKED);else lv_obj_remove_state(slot.favorite,LV_STATE_CHECKED);
      if(trash){lv_obj_add_state(slot.favorite,LV_STATE_USER_1);lv_obj_remove_flag(slot.trash,LV_OBJ_FLAG_HIDDEN);}
      else{lv_obj_remove_state(slot.favorite,LV_STATE_USER_1);lv_obj_add_flag(slot.trash,LV_OBJ_FLAG_HIDDEN);}
      Tab5Focus.describe(slot.favorite,Tab5Locale.text(trash?"Supprimer":"Favori : ajouter ou retirer"));
      if(entries_[index].kind==Message)lv_obj_remove_flag(slot.row,LV_OBJ_FLAG_CLICKABLE);
      else lv_obj_add_flag(slot.row,LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_width(slot.label,std::max<int32_t>(40,width-116));
    lv_obj_set_y(slot.row,int32_t(index*Height));lv_obj_remove_flag(slot.row,LV_OBJ_FLAG_HIDDEN);
    if(index==selected_)lv_obj_add_state(slot.row,LV_STATE_CHECKED);else lv_obj_remove_state(slot.row,LV_STATE_CHECKED);
  }
  rendering_=false;
}
void Tab5MediaList::listEvent(lv_event_t* event) {
  static_cast<Tab5MediaList*>(lv_event_get_user_data(event))->render();
}
void Tab5MediaList::rowEvent(lv_event_t* event) {
  auto* slot=static_cast<Slot*>(lv_event_get_user_data(event));
  if(lv_event_get_code(event)==LV_EVENT_PRESSED){slot->pressed=slot->index;slot->generation=slot->owner->generation_;return;}
  const bool valid=slot->pressed==slot->index&&slot->index!=SIZE_MAX&&slot->generation==slot->owner->generation_;
  slot->pressed=SIZE_MAX;
  if(valid)slot->owner->activate(slot->index,lv_event_get_target_obj(event)==slot->favorite);
}
void Tab5MediaList::activate(size_t index,bool favorite) {
  if(rebuilding_||index>=count_||entries_[index].kind==Message)return;
  selected_=index;
  // The callback may rebuild the model; copy the reference before calling it.
  const Entry entry=entries_[index];auto callback=action_;
  if(callback)callback(entry,favorite);
}
void Tab5MediaList::focus(size_t index,bool favorite) {
  if(index>=count_)return;selected_=index;
  const int32_t y=int32_t(index*Height),top=lv_obj_get_scroll_y(list_),height=lv_obj_get_content_height(list_);
  if(y<top)lv_obj_scroll_to_y(list_,y,LV_ANIM_OFF);
  else if(y+Height>top+height)lv_obj_scroll_to_y(list_,y+Height-height,LV_ANIM_OFF);
  render();for(auto& slot:slots_)if(slot.index==index){Tab5Focus.focusObject(favorite&&!lv_obj_has_flag(slot.favorite,LV_OBJ_FLAG_HIDDEN)?slot.favorite:slot.row);break;}
}
bool Tab5MediaList::key(const writing::KeyEvent& event) {
  using K=writing::Key;if(event.ctrl||event.alt||event.meta||!count_)return false;
  const auto* active=Tab5Focus.focused();bool owns=false,favorite=false;size_t index=selected_;
  for(const auto& slot:slots_)if(active==slot.row||active==slot.favorite){owns=true;favorite=active==slot.favorite;index=slot.index;break;}
  if(!owns||index>=count_)return false;
  const size_t page=std::max<int32_t>(1,lv_obj_get_content_height(list_)/Height);
  switch(event.key) {
    case K::Up:if(index)--index;break;case K::Down:if(index+1<count_)++index;break;
    case K::PageUp:index=index>page?index-page:0;break;
    case K::PageDown:index=std::min(count_-1,index+page);break;
    case K::Home:index=0;break;case K::End:index=count_-1;break;
    case K::Tab:
      if(event.shift){if(!index)return false;--index;}else{if(index+1>=count_)return false;++index;}break;
    case K::Right:focus(index,true);return true;
    case K::Left:if(!favorite)return false;focus(index,false);return true;
    case K::Enter:activate(index,favorite);return true;
    case K::Text:if(std::strcmp(event.text," "))return false;activate(index,favorite);return true;
    default:return false;
  }
  focus(index);return true;
}
