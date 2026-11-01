#pragma once
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cassert>
inline bool leaseOk=true,admitOk=true,allocationOk=true;
inline size_t aliveBytes=0,readCalls=0,admissions=0;
inline std::map<void*,size_t> allocations;
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
constexpr const char* FILE_READ="r";
inline void* heap_caps_malloc(size_t n,unsigned flags){assert(flags==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!allocationOk)return nullptr;auto* p=std::malloc(n);if(p){allocations[p]=n;aliveBytes+=n;}return p;}
inline void heap_caps_free(void* p){if(!p)return;assert(allocations.count(p));aliveBytes-=allocations[p];allocations.erase(p);std::free(p);}
struct Tab5StorageUse{explicit operator bool()const{return leaseOk;}};
namespace Tab5Resources{struct Budget{size_t psram=0,psramBlock=0;};enum class Owner{System};struct Reservation{explicit operator bool()const{return admitOk;}void allocated(size_t internal,size_t){assert(!internal);}};}
inline Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,const Tab5Resources::Budget& b,const char*){++admissions;assert(b.psram==b.psramBlock);return {};}
struct FakeSpec{std::vector<uint8_t> bytes;size_t logical=0;bool directory=false,failRead=false;};
inline std::map<std::string,FakeSpec> files;
struct FakeFile{FakeSpec* spec=nullptr;size_t pos=0;explicit operator bool()const{return spec!=nullptr;}bool isDirectory()const{return spec&&spec->directory;}size_t size()const{return spec->logical?spec->logical:spec->bytes.size();}size_t read(uint8_t* out,size_t n){++readCalls;if(spec->failRead)return 0;n=std::min(n,spec->bytes.size()-pos);memcpy(out,spec->bytes.data()+pos,n);pos+=n;return n;}void close(){}};
struct FakeFS{FakeFile open(const char* p,const char*){auto i=files.find(p);return {i==files.end()?nullptr:&i->second,0};}};
inline FakeFS tab5FS;
