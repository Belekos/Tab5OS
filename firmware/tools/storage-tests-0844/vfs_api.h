#pragma once
#include "FS.h"
class BasicFile:public fs::FileImpl {
 String filename;bool live=true,enumerated=false;
 public:
 BasicFile(const char* path):filename(path){}
 size_t write(const uint8_t*,size_t n)override{return live?n:0;}
 size_t read(uint8_t*,size_t n)override{return live?n:0;}
 void flush()override{}
 bool seek(uint32_t,fs::SeekMode)override{return live;}
 size_t position()const override{return 0;}
 size_t size()const override{return 1;}
 bool setBufferSize(size_t)override{return true;}
 void close()override{live=false;}
 time_t getLastWrite()override{return 0;}
 const char* path()const override{return filename.c_str();}
 const char* name()const override{return filename.c_str();}
 bool isDirectory()override{return filename=="/";}
 fs::FileImplPtr openNextFile(const char*)override{if(enumerated)return {};enumerated=true;return std::make_shared<BasicFile>("/entry.txt");}
 bool seekDir(long)override{return true;}
 String getNextFileName()override{return "/entry.txt";}
 String getNextFileName(bool*)override{return "/entry.txt";}
 void rewindDirectory()override{enumerated=false;}
 operator bool()override{return live;}
};
class VFSImpl:public fs::FSImpl {
 public:
 void mountpoint(const char*){}
 fs::FileImplPtr open(const char* path,const char*,bool)override{return !strcmp(path,"/missing")?fs::FileImplPtr{}:std::make_shared<BasicFile>(path);}
 bool exists(const char*)override{return true;}
 bool remove(const char*)override{return true;}
 bool mkdir(const char*)override{return true;}
 bool rmdir(const char*)override{return true;}
 bool rename(const char*,const char*)override{return true;}
};
