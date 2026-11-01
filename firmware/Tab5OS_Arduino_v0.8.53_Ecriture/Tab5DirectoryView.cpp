#include "Tab5DirectoryView.h"
#include "Tab5VolumeFS.h"
#include "Tab5FilePolicy.h"
#include "Tab5HiddenFiles.h"
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "Tab5Localization.h"
#include <algorithm>

namespace {
lv_color_t color(unsigned index){return lv_color_hex(Tab5UiStyle::color(index));}
uint32_t stamp(){uint32_t hash=Tab5UiStyle::radius();for(unsigned i=0;i<9;++i)hash=hash*33+Tab5UiStyle::color(i);return hash;}
}
Tab5DirectoryView::Tab5DirectoryView(lv_obj_t* list,const lv_font_t* font,Filter filter,Choose choose,void* context)
 :list_(list),font_(font),filter_(filter),choose_(choose),context_(context){
 lv_obj_set_layout(list_,LV_LAYOUT_NONE);lv_obj_set_scroll_dir(list_,LV_DIR_VER);lv_obj_set_scrollbar_mode(list_,LV_SCROLLBAR_MODE_AUTO);
 lv_obj_set_style_pad_all(list_,4,0);lv_obj_set_style_pad_right(list_,18,0);lv_obj_set_style_border_width(list_,0,0);
 lv_obj_add_flag(list_,LV_OBJ_FLAG_SCROLLABLE);lv_obj_remove_flag(list_,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
 extent_=lv_obj_create(list_);lv_obj_remove_style_all(extent_);lv_obj_set_size(extent_,1,1);lv_obj_remove_flag(extent_,LV_OBJ_FLAG_CLICKABLE);
 empty_=lv_label_create(list_);lv_obj_set_pos(empty_,8,8);lv_obj_add_flag(empty_,LV_OBJ_FLAG_HIDDEN);
 for(auto& slot:slots_){slot.owner=this;slot.row=lv_button_create(list_);lv_obj_set_size(slot.row,lv_pct(100),RowHeight-6);lv_obj_remove_flag(slot.row,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(slot.row,0,0);lv_obj_set_style_shadow_width(slot.row,0,0);lv_obj_set_style_border_width(slot.row,1,0);
  slot.icon=lv_label_create(slot.row);lv_obj_set_style_text_font(slot.icon,&lv_font_montserrat_24,0);lv_obj_align(slot.icon,LV_ALIGN_LEFT_MID,12,0);
  slot.label=lv_label_create(slot.row);lv_obj_set_width(slot.label,lv_pct(86));lv_label_set_long_mode(slot.label,LV_LABEL_LONG_DOT);lv_obj_align(slot.label,LV_ALIGN_LEFT_MID,52,0);
  lv_obj_add_flag(slot.row,LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(slot.row,[](lv_event_t* e){auto* slot=static_cast<Slot*>(lv_event_get_user_data(e));slot->pressedIndex=slot->index;slot->pressedGeneration=slot->owner->generation_;},LV_EVENT_PRESSED,&slot);
  lv_obj_add_event_cb(slot.row,[](lv_event_t* e){auto* slot=static_cast<Slot*>(lv_event_get_user_data(e));const bool valid=slot->pressedIndex!=SIZE_MAX&&slot->pressedIndex==slot->index&&slot->pressedGeneration==slot->owner->generation_;slot->pressedIndex=SIZE_MAX;if(valid&&slot->index<slot->owner->model_.size())slot->owner->activate(slot->index);},LV_EVENT_CLICKED,&slot);
 }
 lv_obj_add_event_cb(list_,event,LV_EVENT_SCROLL,this);lv_obj_add_event_cb(list_,event,LV_EVENT_SIZE_CHANGED,this);
 appearance();
}
Tab5DirectoryView::~Tab5DirectoryView(){
 if(sortControl_&&lv_obj_is_valid(sortControl_)){if(Tab5Focus.contains(sortControl_,Tab5Focus.focused()))Tab5Focus.clear();lv_obj_remove_event_cb_with_user_data(sortControl_,event,this);lv_obj_delete(sortControl_);sortControl_=nullptr;}
 directory_.close();if(list_&&lv_obj_is_valid(list_)){if(Tab5Focus.contains(list_,Tab5Focus.focused()))Tab5Focus.clear();lv_obj_remove_event_cb_with_user_data(list_,event,this);lv_obj_clean(list_);}
}
void Tab5DirectoryView::event(lv_event_t* e){auto* self=static_cast<Tab5DirectoryView*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){Tab5FileSort::selected=static_cast<Tab5FileSort::Order>(lv_dropdown_get_selected(self->sortControl_));self->sort();}else self->render();}
lv_obj_t* Tab5DirectoryView::sortControl(lv_obj_t* parent,int width){
 sortControl_=lv_dropdown_create(parent);lv_obj_set_size(sortControl_,width,54);
 String options;for(unsigned i=0;i<6;++i){if(i)options+="\n";options+=Tab5Locale.text(Tab5FileSort::caption(i));}
 lv_dropdown_set_options(sortControl_,options.c_str());lv_dropdown_set_selected(sortControl_,unsigned(Tab5FileSort::selected));
 lv_obj_set_style_text_font(sortControl_,font_,0);Tab5Focus.describe(sortControl_,Tab5Locale.text("Trier par"));Tab5UiStyle::control(sortControl_);
 lv_obj_add_event_cb(sortControl_,event,LV_EVENT_VALUE_CHANGED,this);return sortControl_;
}
void Tab5DirectoryView::sort(){
 order_=Tab5FileSort::selected;if(sortControl_)lv_dropdown_set_selected(sortControl_,unsigned(order_));
 if(scanning_)return;
 String selected=selected_<model_.size()?model_[selected_].name:"";model_.sort(order_);++generation_;
 for(auto& slot:slots_){slot.index=SIZE_MAX;slot.pressedIndex=SIZE_MAX;}
 selected_=SIZE_MAX;for(size_t i=0;i<model_.size();++i)if(selected==model_[i].name){selected_=i;break;}
 render();if(selected_!=SIZE_MAX)focus(selected_);
}
void Tab5DirectoryView::appearance(const lv_font_t* font){
 if(font)font_=font;paletteStamp_=stamp();lv_obj_set_style_bg_color(list_,color(0),0);lv_obj_set_style_bg_opa(list_,LV_OPA_COVER,0);lv_obj_set_style_text_color(list_,color(4),0);
 lv_obj_set_style_bg_color(list_,color(3),LV_PART_SCROLLBAR);lv_obj_set_style_bg_opa(list_,LV_OPA_COVER,LV_PART_SCROLLBAR);
 if(font_){lv_obj_set_style_text_font(list_,font_,0);lv_obj_set_style_text_font(empty_,font_,0);}
 for(auto& slot:slots_){lv_obj_set_style_bg_color(slot.row,color(1),0);lv_obj_set_style_text_color(slot.row,color(4),0);lv_obj_set_style_border_color(slot.row,color(3),0);
  lv_obj_set_style_bg_color(slot.row,color(8),LV_STATE_CHECKED);lv_obj_set_style_bg_color(slot.row,color(8),LV_STATE_PRESSED);lv_obj_set_style_text_color(slot.row,color(4),LV_STATE_CHECKED);
  if(font_)lv_obj_set_style_text_font(slot.label,font_,0);Tab5UiStyle::control(slot.row);
 }
 if(sortControl_)Tab5UiStyle::control(sortControl_);
}
bool Tab5DirectoryView::open(const char* path,bool initialFocus){
 directory_.close();model_.clear();path_=path;++generation_;selected_=SIZE_MAX;followEnd_=false;initialFocus_=initialFocus;error_=nullptr;
 if(Tab5Focus.contains(list_,Tab5Focus.focused()))Tab5Focus.clear();lv_obj_scroll_to_y(list_,0,LV_ANIM_OFF);
 directory_=tab5FS.open(path);scanning_=directory_&&directory_.isDirectory();
 if(!scanning_){directory_.close();error_="Dossier inaccessible";}render();return scanning_;
}
void Tab5DirectoryView::message(const char* text){
 if(text){lv_label_set_text(empty_,Tab5Locale.text(text));lv_obj_remove_flag(empty_,LV_OBJ_FLAG_HIDDEN);}else lv_obj_add_flag(empty_,LV_OBJ_FLAG_HIDDEN);
}
void Tab5DirectoryView::service(){
 if(paletteStamp_!=stamp())appearance();if(order_!=Tab5FileSort::selected)sort();if(!scanning_)return;uint32_t started=millis();bool changed=false;
 for(unsigned step=0;step<12&&millis()-started<5;++step){
  File entry=directory_.openNextFile();if(!entry){directory_.close();scanning_=false;break;}
  const char* raw=entry.name();const char* slash=strrchr(raw,'/');String name=slash?slash+1:raw;bool folder=entry.isDirectory();const uint64_t bytes=entry.size();const int64_t modified=entry.getLastWrite();entry.close();
  if(!name.length()||name=="."||name=="..")continue;
  String full=path_+(path_=="/"?"":"/")+name;
  if(!Tab5Files::showHidden&&Tab5HiddenFiles::hidden(full.c_str()))continue;
  if(filter_&&!filter_(name.c_str(),folder,context_))continue;
  if(!model_.add(name.c_str(),folder,bytes,modified)){directory_.close();scanning_=false;error_="Mémoire insuffisante.";break;}changed=true;
 }
 if(!scanning_)sort();
 if(changed||!scanning_)render();
 if(initialFocus_&&model_.size()){initialFocus_=false;focus(0);}else if(followEnd_&&model_.size())focus(model_.size()-1);
}
void Tab5DirectoryView::render(){
 if(rendering_||!list_)return;rendering_=true;
 const size_t count=model_.size();const int32_t y=lv_obj_get_scroll_y(list_);size_t first=Tab5DirectoryModel::first(count,y,RowHeight,Pool);
 lv_obj_set_pos(extent_,0,count?int32_t(count*RowHeight):0);
 message(error_?error_:scanning_?"Chargement…":!count?"Aucun fichier":nullptr);
 if((error_||scanning_)&&count)lv_obj_set_pos(empty_,8,int32_t(count*RowHeight)+8);else lv_obj_set_pos(empty_,8,8);
 for(unsigned i=0;i<Pool;++i){auto& slot=slots_[i];size_t index=first+i;
  if(index>=count){if(Tab5Focus.focused()==slot.row)Tab5Focus.clear();slot.index=SIZE_MAX;lv_obj_add_flag(slot.row,LV_OBJ_FLAG_HIDDEN);continue;}
  if(slot.index!=index){if(Tab5Focus.focused()==slot.row)Tab5Focus.clear();slot.index=index;const auto& entry=model_[index];lv_label_set_text(slot.label,entry.name);lv_label_set_text(slot.icon,entry.directory?LV_SYMBOL_DIRECTORY:LV_SYMBOL_FILE);Tab5Focus.describe(slot.row,entry.name);}
  lv_obj_set_y(slot.row,int32_t(index*RowHeight));lv_obj_remove_flag(slot.row,LV_OBJ_FLAG_HIDDEN);
  if(index==selected_)lv_obj_add_state(slot.row,LV_STATE_CHECKED);else lv_obj_remove_state(slot.row,LV_STATE_CHECKED);
 }
 rendering_=false;
}
void Tab5DirectoryView::focus(size_t index){
 if(index>=model_.size())return;selected_=index;
 const int32_t y=int32_t(index*RowHeight),top=lv_obj_get_scroll_y(list_),height=lv_obj_get_content_height(list_);
 if(y<top)lv_obj_scroll_to_y(list_,y,LV_ANIM_OFF);else if(y+RowHeight>top+height)lv_obj_scroll_to_y(list_,y+RowHeight-height,LV_ANIM_OFF);
 render();for(auto& slot:slots_)if(slot.index==index){Tab5Focus.focusObject(slot.row);break;}
}
void Tab5DirectoryView::activate(size_t index){
 if(index>=model_.size())return;selected_=index;const auto& item=model_[index];String full=path_+(path_=="/"?"":"/")+item.name;
 const bool directory=item.directory;auto callback=choose_;auto context=context_;if(callback)callback(full.c_str(),directory,context);
}
bool Tab5DirectoryView::key(const writing::KeyEvent& e){
 using K=writing::Key;if(e.ctrl||e.alt||e.meta)return false;
 auto* active=Tab5Focus.focused();bool owns=active==nullptr||active==list_;size_t index=selected_;
 for(const auto& slot:slots_)if(active==slot.row&&slot.index<model_.size()){owns=true;index=slot.index;break;}
 if(!owns||!model_.size())return false;if(index>=model_.size())index=std::min<size_t>(model_.size()-1,std::max<int32_t>(0,lv_obj_get_scroll_y(list_))/RowHeight);
 const size_t page=std::max<int32_t>(1,lv_obj_get_content_height(list_)/RowHeight);followEnd_=false;
 switch(e.key){
  case K::Up:if(index)--index;break;case K::Down:if(index+1<model_.size())++index;break;
  case K::PageUp:index=index>page?index-page:0;break;case K::PageDown:index=std::min(model_.size()-1,index+page);break;
  case K::Home:index=0;break;case K::End:index=model_.size()-1;followEnd_=scanning_;break;
  case K::Tab:
   // Navigate logical entries before LVGL scroll can recycle the focused row.
   // Let the common focus manager reach the toolbar at either list boundary.
   if(!active||active==list_)return false;
   if(e.shift){if(!index)return false;--index;}else{if(index+1>=model_.size())return false;++index;}break;
  case K::Text:if(strcmp(e.text," "))return false;activate(index);return true;
  case K::Enter:activate(index);return true;default:return false;
 }
 focus(index);return true;
}
