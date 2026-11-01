#pragma once
#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Tab5UsbStorage.h"
class String {
 public:
  std::string value;
  String()=default;
  String(const char* s):value(s?s:""){}
  String(const std::string& s):value(s){}
  const char* c_str()const{return value.c_str();}
  size_t length()const{return value.size();}
  int lastIndexOf(char c)const{auto p=value.rfind(c);return p==std::string::npos?-1:int(p);}
  String substring(size_t first)const{return value.substr(first);}
  friend bool operator==(const String& a,const char* b){return a.value==b;}
};
inline uint32_t fakeMilliseconds=0;
inline uint32_t millis(){return fakeMilliseconds++;}
inline int activeHandles=0,peakHandles=0,filesystemOpens=0,reads=0,fileSizeReads=0,fileDateReads=0,directorySizeReads=0,directoryDateReads=0;
inline std::vector<std::string> openedPaths;
struct Node {std::string name;bool directory=false;uint64_t bytes=0;time_t modified=1234;std::vector<std::shared_ptr<Node>> children;};
struct Handle {
  std::shared_ptr<Node> node;size_t cursor=0;
  explicit Handle(std::shared_ptr<Node> n):node(std::move(n)){++activeHandles;if(activeHandles>peakHandles)peakHandles=activeHandles;}
  ~Handle(){--activeHandles;}
};
class File {
  std::shared_ptr<Handle> state;
 public:
  File()=default;
  explicit File(std::shared_ptr<Node> n):state(n?std::make_shared<Handle>(std::move(n)):nullptr){}
  explicit operator bool()const{return bool(state);}
  bool isDirectory()const{return state&&state->node->directory;}
  String name()const{return state?state->node->name:"";}
  time_t getLastWrite()const{if(state){if(state->node->directory)++directoryDateReads;else ++fileDateReads;}return state?state->node->modified:0;}
  uint64_t size()const{if(state){if(state->node->directory)++directorySizeReads;else ++fileSizeReads;}return state?state->node->bytes:0;}
  void close(){state.reset();}
  File openNextFile(){++reads;if(!isDirectory()||state->cursor>=state->node->children.size())return File();return File(state->node->children[state->cursor++]);}
};
inline constexpr int FILE_READ=0;
struct TestFS {
  std::map<std::string,std::shared_ptr<Node>> nodes;
  File open(const char* path,int){++filesystemOpens;openedPaths.push_back(path);if((std::string(path)=="/USB"||std::string(path).find("/USB/")==0)&&!Tab5UsbStorage::isMounted)return File();auto p=nodes.find(path);return p==nodes.end()?File():File(p->second);}
};
inline TestFS tab5FS;
