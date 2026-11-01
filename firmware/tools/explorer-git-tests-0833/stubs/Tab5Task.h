#pragma once
#include "Tab5GitMemory.h"
inline void (*pendingTask)(void*)=nullptr;inline void* pendingTaskArgument=nullptr;
constexpr int pdPASS=1;
inline void tab5TaskCheckpoint(const char*){}
template<class F,class H>int tab5CreateTask(F f,const char*,uint32_t,void* a,unsigned,H,int,Tab5Resources::AdmissionReport*){pendingTask=f;pendingTaskArgument=a;return pdPASS;}
