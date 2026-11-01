#pragma once
#include "Tab5Calculator.h"
#include "Tab5TextInput.h"
#include "Tab5Focus.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tab5calc {
// Text and descriptors are allocated by the caller (PSRAM on the tablet).
// The view has a fixed object pool, independent of the retained history size.
class History {
 public:
 static constexpr unsigned Capacity=128;
 static constexpr size_t TextBudget=64*1024;
 using Allocate=void*(*)(size_t);using Release=void(*)(void*);
 struct Entry {char* expression=nullptr;double value=0;char result[32]{};size_t bytes=0;};
 explicit History(Allocate allocate=std::malloc,Release release=std::free):allocate_(allocate),release_(release){}
 ~History(){clear();}History(const History&)=delete;History& operator=(const History&)=delete;
 unsigned size()const{return count_;}size_t textBytes()const{return bytes_;}unsigned revision()const{return revision_;}
 const Entry* at(unsigned index)const{return entries_&&index<count_?entries_+(first_+index)%Capacity:nullptr;}
 static void format(double value,char* text,size_t size){std::snprintf(text,size,"%.17g",value);}
 bool append(const char* expression,double value){
  if(!expression||std::strlen(expression)>2048)return false;
  const size_t bytes=std::strlen(expression)+1;
  if(!entries_){entries_=static_cast<Entry*>(allocate_(sizeof(Entry)*Capacity));if(!entries_)return false;std::memset(entries_,0,sizeof(Entry)*Capacity);}
  // Allocate before evicting, so a failed allocation preserves the history.
  auto* copy=static_cast<char*>(allocate_(bytes));if(!copy)return false;std::memcpy(copy,expression,bytes);
  while(count_==Capacity||bytes_+bytes>TextBudget)discardFirst();
  auto& entry=entries_[(first_+count_)%Capacity];entry.expression=copy;entry.value=value;entry.bytes=bytes;format(value,entry.result,sizeof(entry.result));++count_;bytes_+=bytes;++revision_;return true;
 }
 void clear(){while(count_)discardFirst();if(entries_)release_(entries_);entries_=nullptr;first_=0;bytes_=0;++revision_;}
 private:
 void discardFirst(){auto& entry=entries_[first_];bytes_-=entry.bytes;release_(entry.expression);entry={};first_=(first_+1)%Capacity;--count_;}
 Allocate allocate_;Release release_;Entry* entries_=nullptr;unsigned first_=0,count_=0,revision_=0;size_t bytes_=0;
};

// Shared by touch keys, the software keyboard and physical keyboard input.
class EntryState {
 public:
 bool result=false;
 void accept(lv_obj_t* input,double value){char text[32];History::format(value,text,sizeof(text));lv_textarea_set_text(input,text);lv_textarea_set_cursor_pos(input,LV_TEXTAREA_CURSOR_LAST);result=true;}
 void insert(lv_obj_t* input,const char* text){
  if(!text||!*text)return;
  if(result){const bool operation=std::strchr("+-*/^%",text[0]);
   if(!operation)lv_textarea_set_text(input,"");
   else{if(text[0]=='^'&&lv_textarea_get_text(input)[0]=='-'){char base[40];std::snprintf(base,sizeof(base),"(%s)",lv_textarea_get_text(input));lv_textarea_set_text(input,base);}auto* label=lv_textarea_get_label(input);lv_label_set_text_selection_start(label,LV_DRAW_LABEL_NO_TXT_SEL);lv_label_set_text_selection_end(label,LV_DRAW_LABEL_NO_TXT_SEL);lv_textarea_set_cursor_pos(input,LV_TEXTAREA_CURSOR_LAST);}
  }
  result=false;Tab5TextInput::insert(input,text);
 }
 void recall(lv_obj_t* input,const char* text){result=false;lv_textarea_set_text(input,text?text:"");lv_textarea_set_cursor_pos(input,LV_TEXTAREA_CURSOR_LAST);}
};

class HistoryView {
 public:
 static constexpr unsigned PoolSize=24;
 using Pick=void(*)(const char*,void*);using Style=void(*)(lv_obj_t*,bool,void*);
 lv_obj_t* create(lv_obj_t* parent,History& model,Pick pick,Style style,void* context){
  model_=&model;pick_=pick;style_=style;context_=context;root_=lv_obj_create(parent);lv_obj_set_style_pad_all(root_,4,0);lv_obj_set_scroll_dir(root_,LV_DIR_VER);lv_obj_set_scrollbar_mode(root_,LV_SCROLLBAR_MODE_AUTO);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLL_CHAIN);lv_obj_set_style_border_width(root_,1,0);
  spacer_=lv_obj_create(root_);lv_obj_remove_style_all(spacer_);lv_obj_remove_flag(spacer_,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_width(spacer_,1);
  for(auto& row:rows_){row.owner=this;row.button=lv_button_create(root_);lv_obj_set_style_pad_all(row.button,6,0);lv_obj_set_style_shadow_width(row.button,0,0);lv_obj_set_style_border_width(row.button,0,0);row.label=lv_label_create(row.button);lv_obj_set_width(row.label,lv_pct(100));lv_label_set_long_mode(row.label,LV_LABEL_LONG_DOT);lv_obj_center(row.label);lv_obj_add_event_cb(row.button,event,LV_EVENT_ALL,&row);}
  lv_obj_add_event_cb(root_,[](lv_event_t* e){auto* self=static_cast<HistoryView*>(lv_event_get_user_data(e));const auto code=lv_event_get_code(e);if(code==LV_EVENT_DELETE){self->root_=self->spacer_=nullptr;return;}if(code==LV_EVENT_SCROLL||code==LV_EVENT_SIZE_CHANGED)self->refresh();},LV_EVENT_ALL,this);
  refresh();return root_;
 }
 lv_obj_t* object()const{return root_;}
 void refresh(bool latest=false){
  if(!root_||refreshing_)return;refreshing_=true;lv_obj_update_layout(root_);const auto* font=lv_obj_get_style_text_font(root_,LV_PART_MAIN);lineHeight_=std::max(44,int(font->line_height)+16);
  const unsigned total=model_->size()*2;lv_obj_set_height(spacer_,std::max(1,int(total)*lineHeight_));
  lv_obj_update_layout(root_);if(latest)lv_obj_scroll_to_y(root_,std::max(0,int(total)*lineHeight_-int(lv_obj_get_content_height(root_))),LV_ANIM_OFF);
  const unsigned first=std::min(total,unsigned(std::max(0,int(lv_obj_get_scroll_y(root_)))/lineHeight_));
  for(unsigned i=0;i<PoolSize;++i){auto& row=rows_[i];unsigned index=first+i;
   if(index>=total){row.line=-1;lv_obj_add_flag(row.button,LV_OBJ_FLAG_HIDDEN);continue;}
   row.line=int(index);lv_obj_remove_flag(row.button,LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(row.button,0,index*lineHeight_);lv_obj_set_size(row.button,std::max(32,int(lv_obj_get_content_width(root_))-6),lineHeight_);
   const auto* entry=model_->at(index/2);lv_label_set_text_static(row.label,index%2?entry->result:entry->expression);lv_obj_set_style_text_align(row.label,index%2?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT,0);if(style_)style_(row.button,index%2,context_);
  }refreshing_=false;
 }
 bool key(const writing::KeyEvent& e){
  if(!root_||e.ctrl||e.meta||e.alt)return false;auto* focused=Tab5Focus.focused();Row* current=nullptr;for(auto& row:rows_)if(row.button==focused){current=&row;break;}if(!current||current->line<0)return false;
  using K=writing::Key;int next=current->line;const int total=int(model_->size()*2);if(!total)return false;
  if(e.key==K::Enter||(e.key==K::Text&&!std::strcmp(e.text," "))){activate(*current,false);return true;}
  if(e.key==K::Tab){next+=e.shift?-1:1;if(next<0||next>=total)return false;}
  else if(e.key==K::Up)--next;else if(e.key==K::Down)++next;else if(e.key==K::Home)next=0;else if(e.key==K::End)next=total-1;else if(e.key==K::PageUp)next-=std::max(1,int(lv_obj_get_content_height(root_))/lineHeight_);else if(e.key==K::PageDown)next+=std::max(1,int(lv_obj_get_content_height(root_))/lineHeight_);else return false;
  next=std::clamp(next,0,total-1);const int y=lv_obj_get_scroll_y(root_),height=lv_obj_get_content_height(root_);if(next*lineHeight_<y)lv_obj_scroll_to_y(root_,next*lineHeight_,LV_ANIM_OFF);else if((next+1)*lineHeight_>y+height)lv_obj_scroll_to_y(root_,(next+1)*lineHeight_-height,LV_ANIM_OFF);refresh();
  for(auto& row:rows_)if(row.line==next){Tab5Focus.focusObject(row.button);break;}return true;
 }
 private:
 struct Row{HistoryView* owner=nullptr;lv_obj_t *button=nullptr,*label=nullptr;int line=-1,pressedLine=-1;unsigned pressedRevision=0;bool pressed=false;};
 static void event(lv_event_t* e){auto* row=static_cast<Row*>(lv_event_get_user_data(e));if(lv_event_get_target_obj(e)!=row->button)return;
  auto code=lv_event_get_code(e);if(code==LV_EVENT_PRESSED){row->pressed=true;row->pressedLine=row->line;row->pressedRevision=row->owner->model_->revision();}
  else if(code==LV_EVENT_CLICKED){row->owner->activate(*row,true);row->pressed=false;}
  else if(code==LV_EVENT_PRESS_LOST)row->pressed=false;
 }
 void activate(Row& row,bool pointer){if(row.line<0||!pick_)return;if(pointer&&row.pressed&&(row.pressedLine!=row.line||row.pressedRevision!=model_->revision()))return;const auto* entry=model_->at(unsigned(row.line)/2);if(entry)pick_(row.line%2?entry->result:entry->expression,context_);}
 History* model_=nullptr;Pick pick_=nullptr;Style style_=nullptr;void* context_=nullptr;lv_obj_t *root_=nullptr,*spacer_=nullptr;Row rows_[PoolSize]{};int lineHeight_=48;bool refreshing_=false;
};
}
