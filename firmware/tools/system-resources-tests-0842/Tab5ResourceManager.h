#pragma once
#include <cstddef>
#include <utility>
namespace admission { inline bool deny=false; inline unsigned active=0; }
namespace Tab5Resources {
enum class Owner {System}; struct Budget {size_t internal=0,internalBlock=0,dmaBlock=0,psram=0,psramBlock=0;};
class Reservation {bool held=false;public: Reservation()=default; explicit Reservation(bool h):held(h){if(h)++admission::active;} ~Reservation(){release();} Reservation(Reservation&& o):held(o.held){o.held=false;} Reservation& operator=(Reservation&& o){release();held=o.held;o.held=false;return *this;} explicit operator bool()const{return held;} void allocated(size_t,size_t){} void release(){if(held)--admission::active;held=false;} };
}
inline Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,const Tab5Resources::Budget&,const char*) {return Tab5Resources::Reservation(!admission::deny);}
