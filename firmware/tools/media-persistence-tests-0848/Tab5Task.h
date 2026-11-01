#pragma once
#include "boundary.h"
#include <algorithm>
#include <cstdlib>
#include <new>
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
constexpr int pdPASS=1,pdFAIL=0,tskNO_AFFINITY=-1;
inline void* heap_caps_malloc(size_t bytes,unsigned caps){
 if(caps!=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))throw std::runtime_error("snapshot capability changed");
 if(Boundary::failAllocation){Boundary::failAllocation=false;return nullptr;}
 auto* p=std::malloc(bytes);if(p){++Boundary::liveJobs;Boundary::maxLiveJobs=std::max(Boundary::maxLiveJobs,Boundary::liveJobs);}return p;
}
inline void heap_caps_free(void* p){if(p){--Boundary::liveJobs;std::free(p);}}
inline void vTaskDelete(void*){}
namespace Tab5Resources {
 enum class Owner{System};
 struct Budget{size_t internal,internalBlock,psram,psramBlock,dma;};
 inline Budget TaskBudget(size_t bytes){return {bytes+4096,bytes,0,0,0};}
 struct Reservation{
  bool held;
  explicit Reservation(bool ok):held(ok){if(held)++Boundary::reservations;}
  Reservation(Reservation&& other):held(other.held){other.held=false;}
  Reservation(const Reservation&)=delete;
  ~Reservation(){release();}
  void release(){if(held){--Boundary::reservations;held=false;}}
  explicit operator bool()const{return held;}
  void allocated(size_t,size_t){}
 };
}
struct MemoryManager {
 Tab5Resources::Reservation reserve(Tab5Resources::Owner,const Tab5Resources::Budget& budget){
  if(budget.internalBlock<4096||budget.psram==0||budget.psramBlock!=budget.psram)throw std::runtime_error("invalid admission");
  return Tab5Resources::Reservation(!Boundary::failAdmission);
 }
};
inline MemoryManager Tab5MemoryManager;
template<class Function,class Handle>
int tab5CreateAdmittedTask(Tab5Resources::Reservation& admission,Function fn,const char*,uint32_t stack,void* arg,unsigned priority,Handle,int core){
 if(core!=tskNO_AFFINITY||priority!=1)throw std::runtime_error("unexpected worker policy");
 if(Boundary::failTask){Boundary::failTask=false;admission.release();return pdFAIL;}
 if(Boundary::worker.joinable())throw std::runtime_error("second worker before join");
 Boundary::stackBytes=stack;++Boundary::launches;admission.release();
 Boundary::worker=std::thread([=]{fn(arg);});return pdPASS;
}
