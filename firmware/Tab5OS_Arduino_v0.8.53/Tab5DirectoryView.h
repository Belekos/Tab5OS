#pragma once
#include <lvgl.h>
#include <FS.h>
#include "WriterKeys.h"
#include "Tab5DirectoryModel.h"

class Tab5DirectoryView {
 public:
  using Filter=bool(*)(const char* name,bool directory,void* context);
  using Choose=void(*)(const char* path,bool directory,void* context);
  explicit Tab5DirectoryView(lv_obj_t* list,const lv_font_t* font,Filter filter,Choose choose,void* context);
  ~Tab5DirectoryView();
  bool open(const char* path,bool initialFocus=true);
  void service();
  void appearance(const lv_font_t* font=nullptr);
  lv_obj_t* sortControl(lv_obj_t* parent,int width=280);
  void sort();
  bool key(const writing::KeyEvent& event);
  bool scanning()const{return scanning_;}
  const char* path()const{return path_.c_str();}
  size_t count()const{return model_.size();}
 private:
  static constexpr unsigned Pool=28,RowHeight=58;
  struct Slot {Tab5DirectoryView* owner=nullptr;lv_obj_t* row=nullptr;lv_obj_t* icon=nullptr;lv_obj_t* label=nullptr;size_t index=SIZE_MAX,pressedIndex=SIZE_MAX;uint32_t pressedGeneration=0;};
  lv_obj_t* list_=nullptr,*extent_=nullptr,*empty_=nullptr;const lv_font_t* font_=nullptr;
  File directory_;String path_;Tab5DirectoryModel model_;Slot slots_[Pool];
  Filter filter_;Choose choose_;void* context_;size_t selected_=SIZE_MAX;
  bool scanning_=false,initialFocus_=false,rendering_=false,followEnd_=false;
  uint32_t paletteStamp_=0,generation_=0;const char* error_=nullptr;
  Tab5FileSort::Order order_=Tab5FileSort::selected;
  lv_obj_t* sortControl_=nullptr;
  void render();void focus(size_t index);void activate(size_t index);void message(const char* text);
  static void event(lv_event_t* event);
};
