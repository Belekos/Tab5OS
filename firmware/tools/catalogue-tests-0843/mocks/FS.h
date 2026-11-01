#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <cstring>
#include <cerrno>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cassert>
#define FILE_READ "r"
namespace MockVolume {
struct Entry{bool directory=false;std::vector<std::string> children;};
inline std::map<std::string,Entry> entries;
inline std::atomic<bool> usb{true},available{true},failOpen{false};
inline std::atomic<unsigned> handles{0},usbOpens{0},leases{0},uiOpens{0},workerOpens{0};
inline std::thread::id uiThread=std::this_thread::get_id();
inline std::mutex gateMutex;inline std::condition_variable gate;
inline std::string blockPath;inline bool armed=false,entered=false,released=false;
inline void arm(const char* path){std::lock_guard<std::mutex> lock(gateMutex);blockPath=path;armed=true;entered=released=false;}
inline void waitBlocked(){std::unique_lock<std::mutex> lock(gateMutex);assert(gate.wait_for(lock,std::chrono::seconds(3),[]{return entered;}));}
inline void release(){std::lock_guard<std::mutex> lock(gateMutex);released=true;gate.notify_all();}
inline void beforeOpen(const char* path){
 if(std::this_thread::get_id()==uiThread)++uiOpens;else ++workerOpens;
 std::unique_lock<std::mutex> lock(gateMutex);
 if(armed&&blockPath==path){armed=false;entered=true;gate.notify_all();gate.wait(lock,[]{return released;});}
}

inline std::string failPath;
inline void add(const std::string& path,bool directory=false){
 entries[path].directory=directory;
 const auto slash=path.rfind('/');if(slash==std::string::npos)return;
 auto parent=path.substr(0,slash);if(parent.empty())parent="/";
 auto& folder=entries[parent];folder.directory=true;folder.children.push_back(path);
}
struct Handle{std::string path;size_t next=0;Handle(const char* p):path(p){++handles;}~Handle(){--handles;}};
}
class File{std::shared_ptr<MockVolume::Handle> state;
 public:
 File()=default;explicit File(const char* p){MockVolume::beforeOpen(p);if(!MockVolume::available){errno=ENODEV;return;}if(MockVolume::failOpen||MockVolume::failPath==p){errno=EIO;return;}if(MockVolume::entries.count(p)){state=std::make_shared<MockVolume::Handle>(p);if(!strncmp(p,"/USB/",5))++MockVolume::usbOpens;}else errno=ENOENT;}
 explicit operator bool()const{return state&&MockVolume::available&&(strncmp(state->path.c_str(),"/USB/",5)||MockVolume::usb);}
 bool isDirectory()const{return bool(*this)&&MockVolume::entries[state->path].directory;}
 const char* name()const{return state?state->path.c_str():"";}
 void close(){state.reset();}
 size_t size()const{return 0;}size_t read(uint8_t*,size_t){return 0;}
 File openNextFile(){if(!isDirectory()){errno=EIO;return {};}
  auto& children=MockVolume::entries[state->path].children;
  if(state->next==children.size()){errno=0;return {};}
  return File(children[state->next++].c_str());}
};
namespace fs{class FS{public:File open(const char* p,const char*){return File(p);}};}
