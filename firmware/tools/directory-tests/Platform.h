#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cstdint>
class String {
 std::string text;
 public:String()=default;String(const char* p):text(p?p:""){}String(const std::string& p):text(p){}
 const char* c_str()const{return text.c_str();}size_t length()const{return text.size();}int lastIndexOf(char c)const{auto n=text.rfind(c);return n==std::string::npos?-1:int(n);}
 String substring(size_t n)const{return text.substr(n);}String substring(size_t n,size_t end)const{return text.substr(n,end-n);}bool startsWith(const char* p)const{return text.compare(0,std::strlen(p),p)==0;}
 String operator+(const String& s)const{return text+s.text;}String& operator+=(const char* s){text+=s;return *this;}bool operator==(const char* s)const{return text==s;}bool operator!=(const char* s)const{return text!=s;}
};
namespace Fake {
inline bool sd=true,usb=true;inline unsigned tick=0,opens=0,reads=0,handles=0;
inline std::map<std::string,std::pair<uint64_t,int64_t>> metadata;
using Entry=std::pair<std::string,bool>;inline std::map<std::string,std::vector<Entry>> folders;
inline bool available(const std::string& p){return p=="/USB"||p.compare(0,5,"/USB/")==0?usb:sd;}
}
class File {
 struct Data {std::string path;bool dir=false;size_t next=0;Data(std::string p,bool d):path(std::move(p)),dir(d){++Fake::handles;}~Data(){--Fake::handles;}};
 std::shared_ptr<Data> data;
 public:File()=default;File(const std::string& p,bool dir):data(std::make_shared<Data>(p,dir)){}
 operator bool()const{return data&&Fake::available(data->path);}bool isDirectory()const{return bool(*this)&&data->dir;}
 const char* name()const{return data?data->path.c_str():"";}void close(){data.reset();}
 uint64_t size()const{return data?Fake::metadata[data->path].first:0;}int64_t getLastWrite()const{return data?Fake::metadata[data->path].second:0;}
 File openNextFile(){++Fake::reads;if(!isDirectory())return {};auto& entries=Fake::folders[data->path];if(data->next>=entries.size())return {};auto entry=entries[data->next++];return {data->path+(data->path=="/"?"":"/")+entry.first,entry.second};}
};
struct FS {File open(const char* p){++Fake::opens;if(!Fake::available(p))return {};if(Fake::folders.count(p))return {p,true};return {};}};
inline FS tab5FS;
inline bool tab5SdAvailable(){return Fake::sd;}
namespace Tab5UsbStorage {inline bool mounted(){return Fake::usb;}}
namespace Tab5HiddenFiles {inline bool hidden(const char* p){return std::strstr(p,"/.")!=nullptr;}}
struct Tab5StorageUse {explicit Tab5StorageUse(bool=false){}operator bool()const{return true;}};
struct Locale {const char* text(const char* p)const{return p;}};
inline Locale Tab5Locale;
inline uint32_t millis(){return Fake::tick;}
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
inline void* heap_caps_malloc(size_t n,unsigned){return std::malloc(n);}inline void* heap_caps_realloc(void* p,size_t n,unsigned){return std::realloc(p,n);}inline void heap_caps_free(void* p){std::free(p);}
#define strcasecmp _stricmp
