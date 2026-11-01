#pragma once
#include <cctype>
#include <cstring>
#include <cstdint>
#include <cstddef>

// One native image catalog for Explorer and Dessin. An extension advertises
// a handler; the decoder still validates the actual file before opening it.
namespace Tab5ImageFormats {
enum class Kind { Unknown, Jpeg, Png, Bmp, Webp, Project };
inline bool extension(const char* path,const char* expected) {
  if(!path)return false;
  const char* dot=std::strrchr(path,'.');
  if(!dot)return false;
  for(;*dot&&*expected;++dot,++expected)
    if(std::tolower(static_cast<unsigned char>(*dot))!=*expected)return false;
  return !*dot&&!*expected;
}
inline Kind kind(const char* path) {
  if(extension(path,".jpg")||extension(path,".jpeg"))return Kind::Jpeg;
  if(extension(path,".png"))return Kind::Png;
  if(extension(path,".bmp"))return Kind::Bmp;
  if(extension(path,".webp"))return Kind::Webp;
  if(extension(path,".t5i"))return Kind::Project;
  return Kind::Unknown;
}
inline Kind sniff(const uint8_t* data,size_t bytes) {
  if(!data)return Kind::Unknown;
  if(bytes>=8&&!std::memcmp(data,"T5IMG001",8))return Kind::Project;
  if(bytes>=12&&!std::memcmp(data,"RIFF",4)&&!std::memcmp(data+8,"WEBP",4))return Kind::Webp;
  if(bytes>=8&&!std::memcmp(data,"\x89PNG\r\n\x1a\n",8))return Kind::Png;
  if(bytes>=2&&data[0]==0xff&&data[1]==0xd8)return Kind::Jpeg;
  if(bytes>=2&&data[0]=='B'&&data[1]=='M')return Kind::Bmp;
  return Kind::Unknown;
}
inline bool supported(const char* path){return kind(path)!=Kind::Unknown;}
}
