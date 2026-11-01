#pragma once
#include <filesystem>
#include <memory>
#include <cstdio>
#include <cstdint>
#include <string>
extern std::string fsRoot;
extern bool writeFailure;
constexpr auto FILE_WRITE="wb";
constexpr auto FILE_READ="rb";
struct File {
 std::shared_ptr<FILE> handle;bool directory=false;bool error=false;
 explicit operator bool()const{return bool(handle)||directory;}
 bool isDirectory()const{return directory;}
 size_t read(uint8_t*p,size_t n){return handle?fread(p,1,n,handle.get()):0;}
 size_t write(const uint8_t*p,size_t n){if(writeFailure){error=true;return 0;}return handle?fwrite(p,1,n,handle.get()):0;}
 bool seek(uint32_t pos){return handle&&fseek(handle.get(),long(pos),SEEK_SET)==0;}
 void flush(){if(handle&&fflush(handle.get()))error=true;}
 bool getWriteError()const{return error;}
 size_t size()const{if(!handle)return 0;long pos=ftell(handle.get());fseek(handle.get(),0,SEEK_END);long bytes=ftell(handle.get());fseek(handle.get(),pos,SEEK_SET);return bytes<0?0:size_t(bytes);}
 void close(){handle.reset();directory=false;}
};
namespace fs{struct FS{
 std::string path(const char*p)const{return fsRoot+p;}
 bool exists(const char*p){return std::filesystem::exists(path(p));}
 bool mkdir(const char*p){return std::filesystem::create_directory(path(p));}
 bool remove(const char*p){return std::filesystem::remove(path(p));}
 bool rename(const char*a,const char*b){std::error_code error;std::filesystem::rename(path(a),path(b),error);return !error;}
 File open(const char*p,const char*mode=FILE_READ){if(std::filesystem::is_directory(path(p)))return {{},true,false};FILE*f=fopen(path(p).c_str(),mode);return {std::shared_ptr<FILE>(f,[](FILE*f){if(f)fclose(f);}),false,false};}
};}
