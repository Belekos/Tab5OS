#pragma once
#include <cstddef>
namespace Tab5Resources {
constexpr size_t KiB=1024,MiB=1024*1024,PsramSafety=512*KiB;
enum class Owner{Image};struct Budget{size_t internal,internalBlock,dma,psram,psramBlock;};
extern bool permit;
class Reservation{public:bool valid=permit;explicit operator bool()const{return valid;}void allocated(size_t,size_t){};};
}
inline Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Budget,const char*){return {};}
