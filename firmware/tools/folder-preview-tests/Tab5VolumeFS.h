#pragma once
#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <vector>

class String {
 public:
  std::string value;
  String() = default;
  String(const char* s) : value(s ? s : "") {}
  String(const std::string& s) : value(s) {}
  const char* c_str() const { return value.c_str(); }
  size_t length() const { return value.size(); }
  int lastIndexOf(char c) const { auto p=value.rfind(c); return p==std::string::npos ? -1 : static_cast<int>(p); }
  String substring(size_t first) const { return value.substr(first); }
  friend bool operator==(const String& a,const char* b) { return a.value==b; }
  friend bool operator==(const String& a,const String& b) { return a.value==b.value; }
  friend bool operator!=(const String& a,const char* b) { return !(a==b); }
  friend String operator+(const String& a,const String& b) { return a.value+b.value; }
};
inline uint32_t fakeMilliseconds=0;
inline uint32_t millis() { return fakeMilliseconds++; }
inline int activeHandles=0,peakHandles=0,filesystemOpens=0;
struct Node {std::string name;bool directory=false;time_t modified=1234;std::vector<std::shared_ptr<Node>> children;};
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
  explicit operator bool()const{return static_cast<bool>(state);}
  bool isDirectory()const{return state&&state->node->directory;}
  String name()const{return state?state->node->name:"";}
  time_t getLastWrite()const{return state?state->node->modified:0;}
  uint64_t size()const{return state?state->node->name.size():0;}
  void close(){state.reset();}
  File openNextFile(){
    if(!isDirectory()||state->cursor>=state->node->children.size())return File();
    return File(state->node->children[state->cursor++]);
  }
};
inline constexpr int FILE_READ=0;
struct TestFS {
  std::map<std::string,std::shared_ptr<Node>> nodes;
  File open(const String& path,int) {++filesystemOpens;auto p=nodes.find(path.value);return p==nodes.end()?File():File(p->second);}
};
inline TestFS tab5FS;
