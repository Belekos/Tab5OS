#pragma once
#include "FS.h"
#include <map>
#include <vector>
namespace FakeVolume {
inline std::string lastPath,renameFrom,renameTo,lastMode;
inline std::map<std::string,std::vector<uint8_t>> files;
}
class BasicFile:public fs::FileImpl {
 String filename,key;bool live=true;size_t pos=0;
 public:
 BasicFile(const char* p,const std::string& k):filename(p),key(k){}
 size_t write(const uint8_t* b,size_t n)override{if(!live)return 0;auto& v=FakeVolume::files[key];if(v.size()<pos+n)v.resize(pos+n);memcpy(v.data()+pos,b,n);pos+=n;return n;}
 size_t read(uint8_t* b,size_t n)override{if(!live)return 0;auto& v=FakeVolume::files[key];size_t count=std::min(n,v.size()-std::min(pos,v.size()));if(count)memcpy(b,v.data()+pos,count);pos+=count;return count;}
 void flush()override{}
 bool seek(uint32_t p,fs::SeekMode)override{pos=p;return live;}
 size_t position()const override{return pos;}
 size_t size()const override{return FakeVolume::files[key].size();}
 bool setBufferSize(size_t)override{return true;}
 void close()override{live=false;}
 time_t getLastWrite()override{return 0;}
 const char* path()const override{return filename.c_str();}
 const char* name()const override{return filename.c_str();}
 bool isDirectory()override{return false;}
 fs::FileImplPtr openNextFile(const char*)override{return {};}
 bool seekDir(long)override{return true;}
 String getNextFileName()override{return {};}
 String getNextFileName(bool*)override{return {};}
 void rewindDirectory()override{}
 operator bool()override{return live;}
};
class VFSImpl:public fs::FSImpl {
 std::string root;
 std::string key(const char* path){return root+path;}
 public:
 void mountpoint(const char* path){root=path;}
 fs::FileImplPtr open(const char* p,const char* mode,bool)override{FakeVolume::lastPath=key(p);FakeVolume::lastMode=mode;if(strchr(mode,'w'))FakeVolume::files[key(p)].clear();if(!FakeVolume::files.count(key(p))&&mode[0]=='r')return {};return std::make_shared<BasicFile>(p,key(p));}
 bool exists(const char* p)override{FakeVolume::lastPath=key(p);return FakeVolume::files.count(key(p))!=0;}
 bool remove(const char* p)override{FakeVolume::lastPath=key(p);return FakeVolume::files.erase(key(p))!=0;}
 bool mkdir(const char* p)override{FakeVolume::lastPath=key(p);FakeVolume::files[key(p)]={};return true;}
 bool rmdir(const char* p)override{return remove(p);}
 bool rename(const char* p,const char* q)override{FakeVolume::renameFrom=key(p);FakeVolume::renameTo=key(q);if(!FakeVolume::files.count(key(p)))return false;FakeVolume::files[key(q)]=FakeVolume::files[key(p)];FakeVolume::files.erase(key(p));return true;}
};
