#pragma once
#include "Arduino.h"
#include <fstream>
#include <memory>
extern std::string testSdRoot,testOverride;
extern size_t testReads,testReadBytes;
#define FILE_READ "r"
class File {
 public:
  File()=default;
  explicit File(const std::string& path):stream(std::make_shared<std::ifstream>(path,std::ios::binary)){
    if(*stream){stream->seekg(0,std::ios::end);bytes=uint32_t(stream->tellg());stream->seekg(0);}
  }
  explicit operator bool()const{return stream&&stream->is_open();}
  bool isDirectory()const{return false;}uint32_t size()const{return bytes;}
  uint32_t position(){return uint32_t(stream->tellg());}
  bool seek(uint32_t n){if(!*this||n>bytes)return false;stream->clear();stream->seekg(n);return bool(*stream);}
  size_t read(uint8_t* out,size_t n){++testReads;if(!*this)return 0;stream->read(reinterpret_cast<char*>(out),n);auto got=size_t(stream->gcount());testReadBytes+=got;return got;}
  int read(){uint8_t x;return read(&x,1)==1?x:-1;}
  void close(){if(stream)stream->close();}
 private:std::shared_ptr<std::ifstream> stream;uint32_t bytes=0;
};
struct TestVolume {File open(const char* path,const char*){return File(testOverride.empty()?testSdRoot+path:testOverride);}File open(const String& path,const char* mode){return open(path.c_str(),mode);}};
extern TestVolume tab5FS;
