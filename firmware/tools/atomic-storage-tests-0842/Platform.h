#pragma once
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
class String:public std::string{public:using std::string::string;String(const std::string& x):std::string(x){}int lastIndexOf(char c)const{auto p=rfind(c);return p==npos?-1:int(p);}String substring(unsigned first,unsigned end=~0u)const{return substr(first,end==~0u?npos:end-first);}bool reserve(size_t n){try{std::string::reserve(n);return true;}catch(...){return false;}}size_t write(uint8_t c){push_back(char(c));return 1;}size_t write(const uint8_t* p,size_t n){append(reinterpret_cast<const char*>(p),n);return n;}};
struct Node {bool directory=false;std::string bytes;};
inline std::map<std::string,std::shared_ptr<Node>> files;
inline int failOperation=-1,operationCount=0;inline std::string statErrorPath,openErrorPath;inline bool statErrorAfterOpen=false,openFailed=false;
inline bool operation(){++operationCount;if(failOperation>=0&&failOperation--==0){errno=EIO;return false;}return true;}
class File {std::shared_ptr<Node> node;size_t position=0;bool error=false;public:File()=default;explicit File(std::shared_ptr<Node> n):node(n){}explicit operator bool()const{return bool(node);}size_t size()const{return node?node->bytes.size():0;}size_t read(uint8_t* out,size_t n){if(!node||!operation())return 0;n=std::min(n,node->bytes.size()-position);memcpy(out,node->bytes.data()+position,n);position+=n;return n;}size_t write(const uint8_t* in,size_t n){if(!node||!operation()){error=true;return 0;}node->bytes.append(reinterpret_cast<const char*>(in),n);return n;}void flush(){operation();}bool getWriteError()const{return error;}void close(){node.reset();}};
constexpr const char* FILE_READ="r";constexpr const char* FILE_WRITE="w";
inline struct FS {
 bool exists(const String& p){if(!operation())return false;if(p==statErrorPath&&(!statErrorAfterOpen||openFailed)){errno=EIO;return false;}if(files.count(p))return true;errno=ENOENT;return false;}
 bool mkdir(const String& p){if(!operation())return false;if(files.count(p)){errno=EEXIST;return false;}auto n=std::make_shared<Node>();n->directory=true;files[p]=n;return true;}
 File open(const String& p,const char* mode=FILE_READ){if(!operation())return {};if(p==openErrorPath){openFailed=true;errno=EIO;return {};}if(*mode=='w'){auto n=std::make_shared<Node>();files[p]=n;return File(n);}auto at=files.find(p);if(at==files.end()){errno=ENOENT;return {};}return File(at->second);}
 bool remove(const String& p){if(!operation())return false;if(files.erase(p))return true;errno=ENOENT;return false;}
 bool rename(const String& from,const String& to){if(!operation())return false;if(files.count(to)){errno=EEXIST;return false;}auto at=files.find(from);if(at==files.end()){errno=ENOENT;return false;}files[to]=at->second;files.erase(at);return true;}
} tab5FS;
inline void tab5StorageFault(const char*,const char*,int=0){}
struct Tab5StorageUse {explicit Tab5StorageUse(bool=true){}explicit operator bool()const{return true;}};
inline uint32_t tab5SdGeneration(){return 1;}inline bool tab5SdAvailable(){return true;}
namespace Tab5UsbStorage {inline uint32_t revision(){return 1;}inline bool mounted(){return true;}inline bool path(const char* p){return !strncmp(p,"/USB/",5);}}
