#pragma once
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <limits>
#include <algorithm>
#include "Tab5FileSort.h"
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

// Directory names live in PSRAM; only a small, fixed pool of LVGL rows is
// created by DirectoryView. No item count or directory depth is imposed.
class Tab5DirectoryModel {
 public:
  struct Entry {char* name;bool directory;uint64_t bytes;int64_t modified;};
  ~Tab5DirectoryModel(){clear();}
  Tab5DirectoryModel()=default;
  Tab5DirectoryModel(const Tab5DirectoryModel&)=delete;
  Tab5DirectoryModel& operator=(const Tab5DirectoryModel&)=delete;
  size_t size()const{return count_;}
  const Entry& operator[](size_t n)const{return entries_[n];}
  bool add(const char* name,bool directory,uint64_t bytes=0,int64_t modified=0){
    if(count_==capacity_){
      size_t next=capacity_?capacity_*2:64;
      if(next<capacity_||next>std::numeric_limits<size_t>::max()/sizeof(Entry))return false;
      auto* grown=static_cast<Entry*>(resize(entries_,next*sizeof(Entry)));if(!grown)return false;
      entries_=grown;capacity_=next;
    }
    const size_t nameBytes=std::strlen(name)+1;auto* copy=static_cast<char*>(resize(nullptr,nameBytes));if(!copy)return false;
    std::memcpy(copy,name,nameBytes);entries_[count_++]={copy,directory,bytes,modified};return true;
  }
  void sort(Tab5FileSort::Order order){if(count_>1)std::sort(entries_,entries_+count_,[order](const Entry& a,const Entry& b){return Tab5FileSort::less(a,b,order);});}
  void clear(){for(size_t i=0;i<count_;++i)release(entries_[i].name);release(entries_);entries_=nullptr;count_=capacity_=0;}
  // Visible rows are contiguous, with one row above and below the viewport.
  static size_t first(size_t count,int32_t scroll,unsigned rowHeight,unsigned pool){
    if(!count)return 0;size_t at=scroll>0?size_t(scroll)/rowHeight:0;if(at)--at;
    return at>count?count:at+pool>count&&count>pool?count-pool:at;
  }
 private:
  Entry* entries_=nullptr;size_t count_=0,capacity_=0;
  static void* resize(void* old,size_t size){
#ifdef ARDUINO
    return heap_caps_realloc(old,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    return std::realloc(old,size);
#endif
  }
  static void release(void* p){
#ifdef ARDUINO
    heap_caps_free(p);
#else
    std::free(p);
#endif
  }
};
