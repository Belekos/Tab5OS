#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <algorithm>
using socklen_t=int;
#define S_ISREG(mode) (((mode)&_S_IFMT)==_S_IFREG)
#define fsync _commit
// ESP-IDF/POSIX descriptors preserve bytes; Windows defaults to CRLF text
// translation even if fdopen later receives "wb". Model target semantics.
inline int sshTestOpen(const char* path,int flags,int mode){return ::_open(path,flags|_O_BINARY,mode);}
#define open sshTestOpen
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
inline size_t sshHostAllocations=0,sshHostPeak=0,sshHostRetained=0,sshHostSessions=0,sshHostAllocationCalls=0;
inline bool sshHostAllocationFailed=false;
inline bool sshFailAllocation(){++sshHostAllocationCalls;const char* at=std::getenv("SSH_FAIL_AT");if(at&&sshHostAllocationCalls==static_cast<size_t>(atoi(at))){sshHostAllocationFailed=true;return true;}return false;}
inline void* heap_caps_malloc(size_t n,int){if(sshFailAllocation()){if(std::getenv("SSH_ALLOC_TRACE"))fprintf(stderr,"ALLOC FAIL %zu %zu\n",sshHostAllocationCalls,n);return nullptr;}auto* p=std::malloc(n);if(p)++sshHostAllocations;if(std::getenv("SSH_ALLOC_TRACE"))fprintf(stderr,"ALLOC %zu %zu %p\n",sshHostAllocationCalls,n,p);return p;}
inline void* heap_caps_aligned_alloc(size_t,size_t n,int flags){return heap_caps_malloc(n,flags);}
inline void* heap_caps_realloc(void* p,size_t n,int){if(!p)return heap_caps_malloc(n,0);if(sshFailAllocation())return nullptr;return std::realloc(p,n);}
inline void heap_caps_free(void* p){if(p){if(std::getenv("SSH_ALLOC_TRACE"))fprintf(stderr,"FREE %p\n",p);--sshHostAllocations;std::free(p);}}
inline auto sshHostStart=std::chrono::steady_clock::now();
inline uint32_t millis(){return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-sshHostStart).count());}
inline void delay(unsigned n){std::this_thread::sleep_for(std::chrono::milliseconds(n));}
inline unsigned sshEmptyConnectPolls=std::getenv("SSH_CONNECT_EMPTY_POLLS")?atoi(std::getenv("SSH_CONNECT_EMPTY_POLLS")):0,sshPrematureWrites=0;
inline int sshTestSelect(int n,fd_set* r,fd_set* w,fd_set* e,const timeval* timeout){if(!r&&w&&sshEmptyConnectPolls){--sshEmptyConnectPolls;delay(20);return 0;}return ::select(n,r,w,e,timeout);}
inline int sshTestSend(SOCKET socket,const char* data,int size,int flags){if(sshEmptyConnectPolls)++sshPrematureWrites;return ::send(socket,data,size,flags);}
#define select sshTestSelect
#define send sshTestSend
inline bool tab5NetworkEnabled(){return std::getenv("SSH_NETWORK_OFF")==nullptr;}
inline void sshTestReleased(size_t retained,size_t peak){sshHostRetained+=retained;sshHostPeak=std::max(sshHostPeak,peak);if(peak)++sshHostSessions;}
inline std::string sshTestVolume(){return std::getenv("SSH_VOLUME");}
class Tab5NetworkLease {public:template<class C>Tab5NetworkLease(const char*,uint32_t,C,void*){}explicit operator bool()const{return std::getenv("SSH_GATE_BUSY")==nullptr;}};
class Preferences {
 std::filesystem::path root;
 public:
 bool begin(const char*,bool){root=std::filesystem::path(sshTestVolume())/"pins";std::error_code e;std::filesystem::create_directory(root,e);return !e;}
 size_t getBytesLength(const char* name){std::error_code e;auto n=std::filesystem::file_size(root/name,e);return e?0:static_cast<size_t>(n);}
 size_t getBytes(const char* name,void* output,size_t size){std::ifstream in(root/name,std::ios::binary);in.read(static_cast<char*>(output),size);return static_cast<size_t>(in.gcount());}
 size_t putBytes(const char* name,const void* input,size_t size){std::ofstream out(root/name,std::ios::binary);out.write(static_cast<const char*>(input),size);out.close();return out?size:0;}
 bool remove(const char* name){std::error_code e;return std::filesystem::remove(root/name,e)&&!e;}
 void end(){}
};
