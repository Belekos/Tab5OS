#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Host adapters replace only storage, PSRAM, preferences and synchronization.
// The test compiles the unmodified production localization/JSON/parser code.
class String {
 public:
  String() = default;
  String(const char* value):value_(value?value:""){}
  String(std::string value):value_(std::move(value)){}
  size_t length() const{return value_.size();}
  const char* c_str() const{return value_.c_str();}
  bool startsWith(const String& prefix) const{return value_.rfind(prefix.value_,0)==0;}
  bool endsWith(const String& suffix) const{return value_.size()>=suffix.value_.size()&&value_.compare(value_.size()-suffix.value_.size(),suffix.value_.size(),suffix.value_)==0;}
  int indexOf(const String& part) const{auto at=value_.find(part.value_);return at==std::string::npos?-1:int(at);}
  int indexOf(char c) const{auto at=value_.find(c);return at==std::string::npos?-1:int(at);}
  void toLowerCase(){for(char& c:value_)c=char(std::tolower(static_cast<unsigned char>(c)));}
  friend bool operator==(const String& a,const String& b){return a.value_==b.value_;}
  friend bool operator!=(const String& a,const String& b){return !(a==b);}
  friend String operator+(const String& a,const String& b){return a.value_+b.value_;}
 private:
  std::string value_;
};

namespace Host {
inline std::map<void*,size_t> allocations;
inline size_t liveBytes=0,peakBytes=0,calls=0,failAt=SIZE_MAX;
inline size_t readLimit=SIZE_MAX,readFailureAt=SIZE_MAX,readCalls=0;
inline bool seekFails=false;
inline void* allocate(size_t bytes){
 if(calls++==failAt)return nullptr;
 void* out=std::malloc(bytes);if(out){allocations[out]=bytes;liveBytes+=bytes;peakBytes=std::max(peakBytes,liveBytes);}return out;
}
inline void release(void* data){if(!data)return;auto found=allocations.find(data);if(found==allocations.end())std::abort();liveBytes-=found->second;allocations.erase(found);std::free(data);}
inline void* resize(void* data,size_t bytes){if(!data)return allocate(bytes);if(!bytes){release(data);return nullptr;}void* out=allocate(bytes);if(out){std::memcpy(out,data,std::min(bytes,allocations.at(data)));release(data);}return out;}
inline std::map<std::string,std::vector<uint8_t>> files;
inline std::map<std::string,std::string> preferences;
inline void put(const std::string& name,const std::string& contents){files[name]={contents.begin(),contents.end()};}
}
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
inline void* heap_caps_malloc(size_t bytes,unsigned caps){if(caps!=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))std::abort();return Host::allocate(bytes);}
inline void* heap_caps_realloc(void* data,size_t bytes,unsigned caps){if(caps!=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))std::abort();return Host::resize(data,bytes);}
inline void heap_caps_free(void* data){Host::release(data);}
using SemaphoreHandle_t=int*;
constexpr uint32_t portMAX_DELAY=UINT32_MAX;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return new int(1);}
inline void vSemaphoreDelete(SemaphoreHandle_t value){delete value;}
inline int xSemaphoreTake(SemaphoreHandle_t,uint32_t){return 1;}
inline int xSemaphoreGive(SemaphoreHandle_t){return 1;}
constexpr const char* FILE_READ="r";

class File {
 struct State {std::string name;const std::vector<uint8_t>* bytes=nullptr;size_t at=0,next=0;bool open=true,directory=false;std::vector<std::string> children;};
 std::shared_ptr<State> state_;
 public:
 File()=default;
 explicit File(const std::string& name){auto i=Host::files.find(name);if(i!=Host::files.end()){state_=std::make_shared<State>();state_->name=name;state_->bytes=&i->second;}else if(name=="/System/languages"){state_=std::make_shared<State>();state_->name=name;state_->directory=true;for(const auto& p:Host::files)if(p.first.rfind(name+"/",0)==0)state_->children.push_back(p.first);}}
 explicit operator bool() const{return state_&&state_->open;}
 bool isDirectory() const{return state_&&state_->directory;}
 size_t size() const{return state_&&state_->bytes?state_->bytes->size():0;}
 const char* name() const{return state_?state_->name.c_str():"";}
 void close(){if(state_)state_->open=false;}
 bool seek(size_t at){if(!*this||Host::seekFails||at>size())return false;state_->at=at;return true;}
 size_t read(uint8_t* out,size_t count){++Host::readCalls;if(!*this||!state_->bytes)return SIZE_MAX;if(state_->at>=Host::readFailureAt)return 0;count=std::min({count,size()-state_->at,Host::readLimit,Host::readFailureAt-state_->at});if(count)std::memcpy(out,state_->bytes->data()+state_->at,count);state_->at+=count;return count;}
 File openNextFile(){if(!isDirectory()||state_->next==state_->children.size())return File();return File(state_->children[state_->next++]);}
};
struct Volume {File open(const char* name,const char* =FILE_READ){return File(name);}File open(const String& name,const char* mode=FILE_READ){return open(name.c_str(),mode);}bool exists(const char* name){return name==std::string("/System/languages")||Host::files.count(name)!=0;}};
inline Volume tab5FS;
class Preferences {
 public:
 bool begin(const char*,bool){return true;}
 void end(){}
 String getString(const char* key,const char* fallback){auto i=Host::preferences.find(key);return i==Host::preferences.end()?String(fallback):String(i->second);}
 size_t putString(const char* key,const String& value){Host::preferences[key]=value.c_str();return value.length();}
};
