#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
struct String:std::string {
 using std::string::string;String()=default;String(const std::string& s):std::string(s){}
 size_t length()const{return size();}
 bool startsWith(const String& s)const{return rfind(s,0)==0;}
 bool endsWith(const String& s)const{return size()>=s.size()&&compare(size()-s.size(),s.size(),s)==0;}
 int indexOf(const char* s)const{auto p=find(s);return p==npos?-1:int(p);}
 int indexOf(char c)const{auto p=find(c);return p==npos?-1:int(p);}
 void toLowerCase(){for(char& c:*this)c=char(std::tolower(static_cast<unsigned char>(c)));}
};
inline std::map<std::string,std::string> fsFiles,prefs;
inline size_t opens=0,reads=0,readBytesCount=0,directoryScans=0,liveBytes=0,peakBytes=0,allocCalls=0,failAllocation=0,ioFailureAt=0,ioLimit=SIZE_MAX;
inline std::map<void*,size_t> allocations;
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
inline void* heap_caps_malloc(size_t n,unsigned caps){assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(++allocCalls==failAllocation)return nullptr;void* p=std::malloc(n);if(p){allocations[p]=n;liveBytes+=n;peakBytes=std::max(peakBytes,liveBytes);}return p;}
inline void heap_caps_free(void* p){if(!p)return;auto i=allocations.find(p);assert(i!=allocations.end());liveBytes-=i->second;allocations.erase(i);std::free(p);}
inline void* heap_caps_realloc(void* p,size_t n,unsigned caps){if(!p)return heap_caps_malloc(n,caps);if(++allocCalls==failAllocation)return nullptr;auto i=allocations.find(p);assert(i!=allocations.end());const size_t old=i->second;void* q=std::realloc(p,n);if(q){allocations.erase(i);allocations[q]=n;liveBytes=liveBytes-old+n;peakBytes=std::max(peakBytes,liveBytes);}return q;}
struct File {
 std::string path;bool valid=false,directory=false;size_t position=0,index=0;
 explicit operator bool()const{return valid;}
 bool isDirectory()const{return directory;}
 size_t size()const{return valid&&!directory?fsFiles.at(path).size():0;}
 const char* name()const{return path.c_str();}
 size_t read(uint8_t* out,size_t n){++reads;if(ioFailureAt&&reads>=ioFailureAt)return size_t(-1);if(!valid||directory)return size_t(-1);const auto& s=fsFiles.at(path);n=std::min({n,s.size()-position,ioLimit});std::memcpy(out,s.data()+position,n);position+=n;readBytesCount+=n;return n;}
 bool seek(size_t offset){if(!valid||directory||offset>size())return false;position=offset;return true;}
 void close(){valid=false;}
 File openNextFile(){++directoryScans;if(!directory||index>=fsFiles.size())return {};auto i=fsFiles.begin();std::advance(i,index++);return {i->first,true,false};}
};
constexpr const char* FILE_READ="r";
struct Fs {File open(const char* path,const char* =FILE_READ){++opens;bool dir=std::string(path)=="/System/languages";return {path,dir||fsFiles.count(path),dir};}bool exists(const char* path){return std::string(path)=="/System/languages"||fsFiles.count(path);}};
inline Fs tab5FS;
struct Preferences {
 bool begin(const char*,bool){return true;}void end(){}
 String getString(const char* key,const char* fallback){auto i=prefs.find(key);return i==prefs.end()?fallback:i->second;}
 size_t putString(const char* key,const String& value){prefs[key]=value;return value.size()+1;}
};
using SemaphoreHandle_t=void*;constexpr int portMAX_DELAY=0;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return reinterpret_cast<void*>(1);}
inline void vSemaphoreDelete(void*){}inline void xSemaphoreTake(void*,int){}inline void xSemaphoreGive(void*){}
