#include <cerrno>
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
struct Data{std::vector<uint8_t> bytes;bool error=false;};
struct Fs;
struct File{
 Fs* fs=nullptr;std::shared_ptr<Data> data;bool opened=false;
 explicit operator bool()const{return opened;}
 size_t write(const uint8_t*,size_t);void flush();
 bool getWriteError()const{return data&&data->error;}
 size_t size()const{return data?data->bytes.size():0;}
 void close(){opened=false;}
};
struct Fs{
 std::map<std::string,std::shared_ptr<Data>> entries;
 bool failMkdir=false,failOpen=false,failRemove=false,failFlush=false;int shortWrite=-1;unsigned renames=0,failRename=0,failRename2=0;
 bool exists(const char* p){if(entries.count(p))return true;errno=ENOENT;return false;}
 bool mkdir(const char* p){if(failMkdir)return false;entries[p]=std::make_shared<Data>();return true;}
 bool remove(const char* p){if(failRemove)return false;return entries.erase(p);}
 bool rename(const char*a,const char*b){++renames;if(renames==failRename||renames==failRename2||!entries.count(a)||entries.count(b))return false;entries[b]=entries[a];entries.erase(a);return true;}
 File open(const char*p,const char*){if(failOpen)return {};auto& d=entries[p];if(!d)d=std::make_shared<Data>();return {this,d,true};}
 std::string text(const char*p){auto i=entries.find(p);if(i==entries.end())return "<missing>";return std::string(i->second->bytes.begin(),i->second->bytes.end());}
 void put(const char*p,const char*v){auto d=std::make_shared<Data>();d->bytes.assign(v,v+strlen(v));entries[p]=d;}
};
size_t File::write(const uint8_t*p,size_t n){if(fs->shortWrite>=0)n=std::min(n,size_t(fs->shortWrite));data->bytes.insert(data->bytes.end(),p,p+n);return n;}
void File::flush(){if(fs->failFlush)errno=EIO;}
