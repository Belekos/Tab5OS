#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
class String:public std::string{public:using std::string::string;String(const std::string& x):std::string(x){};};
inline uint32_t clockMs=1;inline uint32_t millis(){return clockMs;}inline void vTaskDelay(unsigned n){clockMs+=n;}
inline std::vector<std::function<void()>> tasks;
inline bool failTask=false;
template<class F,class H>int tab5CreateTask(F f,const char*,unsigned,void* a,unsigned,H,int){if(failTask)return 0;tasks.emplace_back([=]{f(a);});return 1;}
inline void pump(){while(!tasks.empty()){auto f=tasks.front();tasks.erase(tasks.begin());f();}}
inline void tab5TaskCheckpoint(const char*){}
inline bool diagnosticActive=false;inline std::string diagnosticPhase;inline std::vector<std::string> diagnosticPhases;
inline void tab5DiagnosticMark(unsigned slot,const char* phase,const char* detail,int active){assert(slot==6&&!*detail);diagnosticActive=active;diagnosticPhase=phase;diagnosticPhases.emplace_back(phase);}
inline void vTaskDelete(void*){}
constexpr int pdPASS=1;constexpr unsigned portMAX_DELAY=~0u;
using StaticSemaphore_t=std::mutex;using SemaphoreHandle_t=std::mutex*;
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* p){return p;}
inline void xSemaphoreTake(SemaphoreHandle_t m,unsigned){m->lock();}inline void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
inline std::map<std::string,std::vector<uint8_t>> nvs;inline int failNvs=-1;
inline bool nvsOk(){if(failNvs<0)return true;return failNvs--!=0;}
class Preferences{std::string space;public:bool begin(const char* s,bool=false){space=s;return nvsOk();}void end(){}bool isKey(const char* n){return nvs.count(space+"/"+n)!=0;}size_t getBytesLength(const char* n){if(!nvsOk())return 0;auto i=nvs.find(space+"/"+n);return i==nvs.end()?0:i->second.size();}size_t getBytes(const char* n,void* out,size_t capacity){if(!nvsOk())return 0;auto i=nvs.find(space+"/"+n);if(i==nvs.end()||i->second.size()>capacity)return 0;memcpy(out,i->second.data(),i->second.size());return i->second.size();}size_t putBytes(const char* n,const void* p,size_t size){if(!nvsOk())return 0;auto* bytes=static_cast<const uint8_t*>(p);nvs[space+"/"+n]=std::vector<uint8_t>(bytes,bytes+size);return size;}bool remove(const char* n){return nvsOk()&&nvs.erase(space+"/"+n);}};
struct Node{bool directory=false;std::vector<uint8_t> data;};
inline std::map<std::string,std::shared_ptr<Node>> files;inline int failFs=-1,fsOps=0;inline bool failRandom=false;
inline std::function<void()> fsObserver;
inline bool fsOk(){++fsOps;if(fsObserver)fsObserver();if(failFs<0)return true;return failFs--!=0;}
class File{std::shared_ptr<Node> node;size_t pos=0;public:File()=default;explicit File(std::shared_ptr<Node> n):node(n){}explicit operator bool()const{return bool(node);}bool isDirectory()const{return node&&node->directory;}size_t size()const{return node?node->data.size():0;}size_t read(uint8_t* out,size_t n){if(!node||!fsOk())return 0;n=std::min(n,node->data.size()-std::min(pos,node->data.size()));memcpy(out,node->data.data()+pos,n);pos+=n;return n;}size_t write(const uint8_t* in,size_t n){if(!node||!fsOk())return 0;if(pos+n>node->data.size())node->data.resize(pos+n);memcpy(node->data.data()+pos,in,n);pos+=n;return n;}bool seek(size_t p){if(!node||p>node->data.size()||!fsOk())return false;pos=p;return true;}void flush(){}void close(){node.reset();}};
constexpr const char* FILE_READ="r";constexpr const char* FILE_WRITE="w";
struct FileSystem{File open(const String& p,const char* mode=FILE_READ){if(!fsOk())return {};auto i=files.find(p);if(*mode=='w'){auto n=std::make_shared<Node>();files[p]=n;return File(n);}return i==files.end()?File():File(i->second);}bool exists(const String& p){return files.count(p)!=0;}bool mkdir(const char* p){if(!fsOk())return false;auto n=std::make_shared<Node>();n->directory=true;files[p]=n;return true;}bool remove(const String& p){return fsOk()&&files.erase(p)!=0;}bool rename(const String& a,const String& b){if(!fsOk()||!files.count(a)||files.count(b))return false;std::map<std::string,std::shared_ptr<Node>> moved;for(const auto& item:files)if(item.first==a||item.first.compare(0,a.size()+1,std::string(a)+"/")==0)moved[std::string(b)+item.first.substr(a.size())]=item.second;for(auto it=files.begin();it!=files.end();)if(it->first==a||it->first.compare(0,a.size()+1,std::string(a)+"/")==0)it=files.erase(it);else ++it;files.insert(moved.begin(),moved.end());return true;}};
inline FileSystem tab5FS;
inline bool sdAvailable=true;inline bool tab5SdAvailable(){return sdAvailable;}
struct Tab5StorageUse{explicit operator bool()const{return true;}};
namespace Tab5CryptoRandom{inline bool fill(void* out,size_t n){if(failRandom)return false;static uint32_t seed=123456;auto* p=static_cast<uint8_t*>(out);for(size_t i=0;i<n;++i){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;p[i]=uint8_t(seed);}return true;}}
namespace Tab5Files{inline bool canonical(const char* p,std::string& result){if(!p||*p!='/')return false;result=p;return result.find("..") ==std::string::npos;}}
