#pragma once
#include <string>
#include <map>
#include <memory>
#include <cstdint>
#include <cstring>
#include <algorithm>
class String: public std::string {
 public:
 using std::string::string;
 bool reserve(size_t n){try{std::string::reserve(n);return true;}catch(...){return false;}}
 size_t write(uint8_t c){push_back(char(c));return 1;}
 size_t write(const uint8_t* p,size_t n){append(reinterpret_cast<const char*>(p),n);return n;}
};
inline std::map<std::string,std::string> disk;
inline bool blocked=false,failWrite=false;
inline uint32_t sdGeneration=1,usbGeneration=1;
inline bool sdMounted=true,usbMounted=true;
struct File {
 std::shared_ptr<std::string> content;size_t offset=0;
 operator bool()const{return bool(content);}
 size_t size()const{return content?content->size():0;}
 size_t read(uint8_t* dest,size_t count){if(!content)return size_t(-1);count=std::min(count,content->size()-offset);memcpy(dest,content->data()+offset,count);offset+=count;return count;}
};
inline struct FS {
 bool exists(const char* name){return disk.count(name)!=0;}
 File open(const char* name,const char*){auto it=disk.find(name);return {it==disk.end()?nullptr:std::make_shared<std::string>(it->second)};}
} tab5FS;
#define FILE_READ "r"
struct Tab5StorageUse {Tab5StorageUse(bool){} operator bool()const{return !blocked;}};
inline bool tab5SdAvailable(){return sdMounted;}
inline uint32_t tab5SdGeneration(){return sdGeneration;}
