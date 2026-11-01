#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>
#if defined(ARDUINO) && !defined(TAB5_SHELL_HOST)
#include <esp_heap_caps.h>
#endif
namespace Tab5Shell {
// Large, short-lived pipe/substitution data must never consume internal SRAM.
class Buffer {
 char* data_=nullptr;size_t size_=0,capacity_=0,limit_;
 public:
 explicit Buffer(size_t limit):limit_(limit){}
 Buffer(const Buffer&)=delete;Buffer& operator=(const Buffer&)=delete;
 Buffer(Buffer&& b)noexcept:limit_(b.limit_){swap(b);}
 Buffer& operator=(Buffer&& b)noexcept{swap(b);return *this;}
 ~Buffer(){release();}
 void release(){
#if defined(ARDUINO) && !defined(TAB5_SHELL_HOST)
  heap_caps_free(data_);
#else
  std::free(data_);
#endif
  data_=nullptr;size_=capacity_=0;
 }
 void swap(Buffer& b)noexcept{std::swap(data_,b.data_);std::swap(size_,b.size_);std::swap(capacity_,b.capacity_);std::swap(limit_,b.limit_);}
 bool append(const char* p,size_t n){if(n>limit_-size_)return false;if(!n)return true;
  if(size_+n+1>capacity_){size_t next=std::min(limit_+1,std::max(size_+n+1,std::max(size_t(64),capacity_*2)));
#if defined(ARDUINO) && !defined(TAB5_SHELL_HOST)
   void* data=heap_caps_realloc(data_,next,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
   void* data=std::realloc(data_,next);
#endif
   if(!data)return false;data_=static_cast<char*>(data);capacity_=next;
  }memcpy(data_+size_,p,n);size_+=n;data_[size_]=0;return true;
 }
 const char* data()const{return data_?data_:"";}size_t size()const{return size_;}
 void trimNewlines(){while(size_&&data_[size_-1]=='\n')--size_;if(data_)data_[size_]=0;}
};
}
