#pragma once
#include "Arduino.h"
#include <map>
#include <set>
#include <memory>
#include <cerrno>
#define FILE_READ "r"
#define FILE_WRITE "w"
namespace mock {
inline std::map<std::string,std::string> files;
inline std::set<std::string> dirs{"/"};
inline bool shortWrite=false,corruptRead=false,renameFailure=false;
inline unsigned writes=0;
struct Handle {std::string path;size_t pos=0;bool directory=false;bool error=false;};
}
class File {
 std::shared_ptr<mock::Handle> handle;
 public:
 File()=default;
 File(const std::string& path,bool directory):handle(std::make_shared<mock::Handle>()){handle->path=path;handle->directory=directory;}
 explicit operator bool()const{return bool(handle);}
 bool isDirectory()const{return handle&&handle->directory;}
 size_t size()const{return handle&&!isDirectory()?mock::files[handle->path].size():0;}
 void close(){handle.reset();}
 void flush(){}
 bool getWriteError(){return handle&&handle->error;}
 size_t write(const uint8_t* data,size_t count){
  if(!handle||isDirectory())return 0;
  ++mock::writes;
  if(mock::shortWrite&&count){--count;handle->error=true;}
  auto& target=mock::files[handle->path];target.append(reinterpret_cast<const char*>(data),count);handle->pos+=count;return count;
 }
 size_t read(uint8_t* data,size_t count){
  if(!handle||isDirectory())return 0;
  auto& source=mock::files[handle->path];count=std::min(count,source.size()-handle->pos);
  memcpy(data,source.data()+handle->pos,count);handle->pos+=count;
  if(mock::corruptRead&&count)data[0]^=1;
  return count;
 }
};
namespace fs { class FS {
 public:
 bool exists(const String& path){return mock::files.count(path)||mock::dirs.count(path);}
 File open(const String& path,const char* mode=FILE_READ){
  if(mock::dirs.count(path))return File(path,true);
  if(!strcmp(mode,FILE_WRITE)){mock::files[path]="";return File(path,false);}
  return mock::files.count(path)?File(path,false):File();
 }
 bool mkdir(const String& path){if(mock::files.count(path))return false;mock::dirs.insert(path);return true;}
 bool remove(const String& path){return mock::files.erase(path)>0;}
 bool rename(const String& from,const String& to){
  if(mock::renameFailure||exists(to)||!mock::files.count(from))return false;
  mock::files[to]=mock::files[from];mock::files.erase(from);return true;
 }
}; }
