#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <algorithm>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <filesystem>
#include <fstream>
#include <cassert>
#ifndef S_ISREG
#define S_ISREG(mode) (((mode)&_S_IFMT)==_S_IFREG)
#endif
class String {std::string data;public:String(const char* s=""):data(s){}String(const std::string& s):data(s){}const char* c_str()const{return data.c_str();}size_t length()const{return data.size();}};
class Stream {public:virtual ~Stream()=default;virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t*,size_t)=0;virtual int available()=0;virtual int read()=0;virtual int peek()=0;virtual void flush()=0;};
namespace HttpFake {
struct Reply{int status=200;std::string body;std::map<std::string,std::string> headers;bool truncated=false;};
inline std::filesystem::path root;
inline std::vector<Reply> replies;
inline std::vector<std::string> urls;
inline uint32_t tick=0,cancelAt=0,step=1,largestTimeout=0;
inline bool enabled=true,clock=true,usb=true,sd=true;
inline int fdCount=0,failWriteAfter=-1;
inline size_t wrote=0;inline unsigned secureRequests=0;
inline std::string local(const char* name){std::string p=name;if(p.compare(0,3,"/sd")==0)p=p.substr(3);return (root/p.substr(p.empty()||p[0]!='/'?0:1)).string();}
inline void reset(){replies.clear();urls.clear();tick=cancelAt=largestTimeout=0;step=1;enabled=clock=usb=sd=true;failWriteAfter=-1;wrote=secureRequests=0;assert(fdCount==0);}
}
inline uint32_t millis(){return HttpFake::tick;}
inline bool tab5NetworkEnabled(){return HttpFake::enabled;}
inline bool tab5SdAvailable(){return HttpFake::sd;}
inline void tab5FsChanged(){}
namespace Tab5UsbStorage {inline bool mounted(){return HttpFake::usb;}}
struct Tab5StorageUse {operator bool()const{return true;}};
struct NetworkClient {virtual ~NetworkClient()=default;void setTimeout(unsigned n){HttpFake::largestTimeout=std::max(HttpFake::largestTimeout,n);}void setHandshakeTimeout(unsigned n){HttpFake::largestTimeout=std::max(HttpFake::largestTimeout,n*1000);}virtual bool secure()const{return false;}};
struct Tab5SecureClient:NetworkClient {static bool clockReady(){return HttpFake::clock;}bool secure()const override{return true;}};
template<class Base>struct Tab5CooperativeClient:Base {Tab5CooperativeClient(bool(*)(void*),void*){}};
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS=0;
class HTTPClient {
 HttpFake::Reply reply;bool secure=false;
 public:
 void setConnectTimeout(unsigned n){HttpFake::largestTimeout=std::max(HttpFake::largestTimeout,n);}void setTimeout(unsigned n){HttpFake::largestTimeout=std::max(HttpFake::largestTimeout,n);}void setFollowRedirects(int){}void setReuse(bool){}
 bool begin(NetworkClient& c,const char* url){secure=c.secure();if(secure)++HttpFake::secureRequests;HttpFake::urls.push_back(url);return true;}
 void collectHeaders(const char*[],size_t){}void addHeader(const char*,const char*){}
 int sendRequest(const char*){return GET();}
 int GET(){if(HttpFake::replies.empty())return -1;reply=HttpFake::replies.front();HttpFake::replies.erase(HttpFake::replies.begin());HttpFake::tick+=HttpFake::step;return reply.status;}
 String header(const char* name){return reply.headers[name];}bool hasHeader(const char* name){return !reply.headers[name].empty();}
 int writeToStream(Stream* sink){size_t sent=0;while(sent<reply.body.size()){HttpFake::tick+=HttpFake::step;size_t n=std::min(size_t(32),reply.body.size()-sent);size_t done=sink->write(reinterpret_cast<const uint8_t*>(reply.body.data()+sent),n);if(done!=n)return -1;sent+=n;}return reply.truncated?-1:int(sent);}
 void end(){}
};
inline int fakeFileOpen(const char* path,int flags,int mode){int fd=::_open(HttpFake::local(path).c_str(),flags|_O_BINARY,mode);if(fd>=0)++HttpFake::fdCount;return fd;}
inline int fakeFileClose(int fd){--HttpFake::fdCount;return ::_close(fd);}
inline int fakeFileWrite(int fd,const void* data,unsigned size){if(HttpFake::failWriteAfter>=0&&HttpFake::wrote>=unsigned(HttpFake::failWriteAfter))return -1;int n=::_write(fd,data,size);if(n>0)HttpFake::wrote+=n;return n;}
inline int fakeFileAccess(const char* p,int mode){return ::_access(HttpFake::local(p).c_str(),mode);}
inline int fakeFileUnlink(const char* p){return ::_unlink(HttpFake::local(p).c_str());}
inline int fakeFileRename(const char* p,const char* q){return ::rename(HttpFake::local(p).c_str(),HttpFake::local(q).c_str());}
inline int fakeFileStat(const char* p,struct stat* out){return ::stat(HttpFake::local(p).c_str(),out);}
using ssize_t=long long;
#define F_OK 0
#define open fakeFileOpen
#define close fakeFileClose
#define write fakeFileWrite
#define access fakeFileAccess
#define unlink fakeFileUnlink
#define rename fakeFileRename
#define fsync _commit
// Function-like macro leaves `struct stat` intact.
#define stat(...) fakeFileStat(__VA_ARGS__)
