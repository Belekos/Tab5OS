#pragma once
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
class String:public std::string{public:using std::string::string;String(const std::string& x):std::string(x){}int lastIndexOf(char c)const{auto p=rfind(c);return p==npos?-1:int(p);}String substring(unsigned first,unsigned end=~0u)const{return substr(first,end==~0u?npos:end-first);}};
struct Node {bool directory=false;std::string bytes;};
inline std::map<std::string,std::shared_ptr<Node>> files;
inline std::string failStage;inline int failError=0;inline bool cleanupFails=false,promoteFails=false;inline unsigned mutations=0;
inline bool failing(const char* stage){if(failStage==stage){errno=failError;return true;}errno=0;return false;}
class File {std::shared_ptr<Node> node;bool error=false;public:
 File()=default;explicit File(std::shared_ptr<Node> n):node(n){}explicit operator bool()const{return bool(node);}
 size_t size()const{if(failing("size"))return 0;return node?node->bytes.size():0;}
 size_t write(const uint8_t* in,size_t n){if(failing("write")){error=true;const size_t shortCount=std::min(size_t(13),n);if(node)node->bytes.append(reinterpret_cast<const char*>(in),shortCount);return shortCount;}if(!node)return 0;node->bytes.append(reinterpret_cast<const char*>(in),n);return n;}
 void flush(){failing("flush");}bool getWriteError()const{return error;}
 void close(){node.reset();failing("close");}
};
constexpr const char* FILE_WRITE="w";
inline struct FS {
 bool exists(const String& p){if(failing("probe"))return false;if(files.count(p))return true;errno=ENOENT;return false;}
 bool mkdir(const String& p){++mutations;if(files.count(p)){errno=EEXIST;return false;}auto n=std::make_shared<Node>();n->directory=true;files[p]=n;return true;}
 File open(const String& p,const char*){++mutations;auto n=std::make_shared<Node>();files[p]=n;return File(n);}
 bool remove(const String& p){++mutations;if(cleanupFails){errno=EACCES;return false;}if(files.erase(p)){errno=0;return true;}errno=ENOENT;return false;}
 bool rename(const String& from,const String& to){++mutations;if(promoteFails&&from=="/docs/.a.txt.tmp"){errno=ENOSPC;return false;}if(files.count(to)){errno=EEXIST;return false;}auto at=files.find(from);if(at==files.end()){errno=ENOENT;return false;}files[to]=at->second;files.erase(at);errno=0;return true;}
} tab5FS;
