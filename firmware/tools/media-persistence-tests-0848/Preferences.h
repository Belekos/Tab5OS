#pragma once
#include "boundary.h"
class Preferences {
 public:
  bool begin(const char* name,bool=false){if(std::strcmp(name,"tab5_media"))throw std::runtime_error("unexpected namespace");return Boundary::available;}
  void end(){}
  size_t getBytesLength(const char* key){auto i=Boundary::values.find(key);return i==Boundary::values.end()?0:i->second.size();}
  size_t getBytes(const char* key,void* out,size_t n){auto i=Boundary::values.find(key);if(i==Boundary::values.end()||n<i->second.size())return 0;std::memcpy(out,i->second.data(),i->second.size());return i->second.size();}
  size_t putBytes(const char* key,const void* input,size_t size){
    Boundary::wait();
    ++Boundary::attempts;if(std::this_thread::get_id()==Boundary::ui)++Boundary::uiWrites;
    Boundary::clockUs+=5000;
    if(!Boundary::writable||(Boundary::failBrowser&&!std::strcmp(key,"browser"))||(Boundary::failVolume&&!std::strcmp(key,"volume")))return 0;
    const auto* bytes=static_cast<const uint8_t*>(input);
    Boundary::values[key]={bytes,bytes+size};++Boundary::writes;Boundary::bytesWritten+=size;return size;
  }
};
