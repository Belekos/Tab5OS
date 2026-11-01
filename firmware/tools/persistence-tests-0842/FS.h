#pragma once
#include "Arduino.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <memory>
#include <cerrno>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#define FILE_READ "r"
#define FILE_WRITE "w"
namespace fs {
class FS;
struct ReadStall{};
class File {
 struct State {FS* fs=nullptr;std::string path;std::vector<std::string> names;size_t cursor=0;~State();};
 std::shared_ptr<State> state;bool writeFailed=false;mutable unsigned availablePolls=0;
 public:
 File()=default;
 File(FS* owner,const std::string& path);
 operator bool()const{return bool(state);}
 bool isDirectory()const;
 const char* name()const{return state?state->path.c_str():nullptr;}
 void close();
 File openNextFile(const char*);
 size_t position()const;
 size_t size()const;
 int read();
 bool available()const{if(++availablePolls>4096)throw ReadStall{};return state&&position()<size();}
 String readStringUntil(char delimiter){String result;int c;while((c=read())>=0&&c!=delimiter)result+=char(c);return result;}
 size_t write(const uint8_t*,size_t);
 template<class T>size_t printf(const char* format,T value){int n=std::snprintf(nullptr,0,format,value);if(n<0)return 0;std::vector<char> bytes(n+1);std::snprintf(bytes.data(),bytes.size(),format,value);return write(reinterpret_cast<const uint8_t*>(bytes.data()),n);}
 void flush();
 bool getWriteError()const{return writeFailed;}
};
struct PowerCut{};
class FS {
 public:
 struct Node {bool directory=true;std::string contents;};
 std::map<std::string,Node> tree{{"/",{}}};
 unsigned operations=0,mutations=0,enums=0,failAt=0,cutAfter=0;
 unsigned handles=0,maxHandles=0;
 bool denyAll=false;
 size_t failReadAfter=SIZE_MAX;
 bool positionError=false,sizeError=false,flushError=false,closeError=false;
 size_t failWriteAfter=SIZE_MAX;unsigned renameCalls=0,failRenameAt=0;
 bool fail(){++operations;if(denyAll||operations==failAt){errno=EIO;return true;}return false;}
 static std::string parent(const std::string& p){auto slash=p.rfind('/');return slash==0?"/":p.substr(0,slash);}
 void mutated(){++mutations;if(cutAfter&&mutations==cutAfter)throw PowerCut{};}
 bool exists(const char* path){if(fail())return false;const bool present=tree.count(path);if(!present)errno=ENOENT;return present;}
 File open(const char* path,const char* mode=FILE_READ){if(fail())return {};if(mode[0]=='w'){if(!tree.count(parent(path))){errno=ENOENT;return {};}tree[path]={false,""};mutated();}if(!tree.count(path)){errno=ENOENT;return {};}return File(this,path);}
 bool mkdir(const char* path){if(fail())return false;std::string p=path;if(tree.count(p)||!tree.count(parent(p))||!tree[parent(p)].directory){errno=EEXIST;return false;}tree[p]={};mutated();return true;}
 bool rename(const char* from,const char* to){
  if(fail()||++renameCalls==failRenameAt)return false;std::string a=from,b=to;
  if(!tree.count(a)||!tree.count(parent(b))||!tree[parent(b)].directory){errno=ENOENT;return false;}
  if(b.rfind(a+"/",0)==0){errno=EINVAL;return false;}
  // Native POSIX rename may replace files. The production migration must
  // prevent reaching this behavior through its own checked destination probe.
  if(tree.count(b)){if(tree[b].directory||tree[a].directory){errno=EEXIST;return false;}tree.erase(b);}
  std::vector<std::pair<std::string,Node>> moving;
  for(const auto& n:tree)if(n.first==a||n.first.rfind(a+"/",0)==0)moving.push_back(n);
  for(const auto& n:moving)tree.erase(n.first);
  for(const auto& n:moving)tree[b+n.first.substr(a.size())]=n.second;
  mutated();return true;
 }
 bool remove(const char* path){if(fail())return false;if(!tree.count(path)){errno=ENOENT;return false;}if(tree[path].directory){errno=EISDIR;return false;}tree.erase(path);mutated();return true;}
 void seedDir(const std::string& p){if(p!="/"&&!tree.count(parent(p)))seedDir(parent(p));tree[p]={};}
 void seedFile(const std::string& p,const std::string& value){seedDir(parent(p));tree[p]={false,value};}
 std::multiset<std::string> data()const{std::multiset<std::string> values;for(const auto& n:tree)if(!n.second.directory)values.insert(n.second.contents);return values;}
 void clearFaults(){operations=mutations=enums=failAt=cutAfter=0;denyAll=false;failReadAfter=SIZE_MAX;positionError=sizeError=false;}
};
inline File::File(FS* owner,const std::string& path){
 state=std::make_shared<State>();state->fs=owner;state->path=path;
 ++owner->handles;owner->maxHandles=std::max(owner->handles,owner->maxHandles);
 if(owner->tree[path].directory)for(const auto& n:owner->tree)if(n.first!=path&&FS::parent(n.first)==path)state->names.push_back(n.first);
}
inline File::State::~State(){if(fs)--fs->handles;}
inline bool File::isDirectory()const{return state&&state->fs->tree.count(state->path)&&state->fs->tree[state->path].directory;}
inline File File::openNextFile(const char*){
 if(!state)return {};auto& owner=*state->fs;++owner.enums;if(owner.fail())return {};
 while(state->cursor<state->names.size()){auto path=state->names[state->cursor++];if(owner.tree.count(path))return File(&owner,path);}
 return {};
}
inline size_t File::position()const{if(!state)return 0;return state->fs->positionError?SIZE_MAX:state->cursor;}
inline size_t File::size()const{if(!state)return 0;return state->fs->sizeError?SIZE_MAX:state->fs->tree[state->path].contents.size();}
inline int File::read(){if(!state)return -1;if(state->cursor>=state->fs->failReadAfter){errno=EIO;return -1;}auto& data=state->fs->tree[state->path].contents;if(state->cursor>=data.size())return -1;return uint8_t(data[state->cursor++]);}
inline size_t File::write(const uint8_t* bytes,size_t count){if(!state)return 0;if(state->cursor>=state->fs->failWriteAfter){writeFailed=true;errno=EIO;return 0;}count=std::min(count,state->fs->failWriteAfter-state->cursor);auto& data=state->fs->tree[state->path].contents;data.append(reinterpret_cast<const char*>(bytes),count);state->cursor+=count;return count;}
}
inline void fs::File::flush(){if(state&&state->fs->flushError){errno=EIO;}}
inline void fs::File::close(){bool failed=state&&state->fs->closeError;state.reset();if(failed)errno=EIO;}
using fs::File;
