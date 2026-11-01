#pragma once
#include "Tab5Git.h"
namespace Tab5Resources {
enum class Owner{Tools};enum class Denial{None,Psram};struct AdmissionReport{Denial reason=Denial::None;};struct Budget{size_t internal,internalBlock,dma,psram,psramBlock;};
class Reservation{public:bool valid=true;explicit operator bool()const{return valid;}void release(){valid=false;}void allocated(size_t,size_t){}};
inline void describeAdmission(const AdmissionReport&,char* p,size_t n){snprintf(p,n,"denied");}
}
inline Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Budget,const char*){return {};}
namespace Tab5Git{inline Tab5Resources::Reservation reserveStartup(const std::vector<std::string>&,Tab5Resources::AdmissionReport*){return {};}}
