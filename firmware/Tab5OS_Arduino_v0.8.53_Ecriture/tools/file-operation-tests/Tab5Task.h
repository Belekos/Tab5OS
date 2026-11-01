#pragma once
#include <stddef.h>
namespace Tab5Resources {
enum class Owner { Files };
struct Budget { size_t internal, internalBlock, dmaBlock, psram, psramBlock; };
struct Reservation { explicit operator bool() const { return true; } void allocated(size_t,size_t) {} };
}
inline Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,const Tab5Resources::Budget&,const char*) { return {}; }
inline void tab5TaskCheckpoint(const char*) {}
#define pdPASS 1
inline int tab5CreateAdmittedTask(Tab5Resources::Reservation&,void(*f)(void*),const char*,unsigned,void* argument,unsigned,void*,int) { f(argument); return pdPASS; }
