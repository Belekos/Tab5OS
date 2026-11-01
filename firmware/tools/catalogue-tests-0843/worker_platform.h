#pragma once
#include "Tab5ResourceManager.h"
#include "freertos/task.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include <vector>
#include <mutex>
#include <condition_variable>
namespace MockTask {
inline bool admit=false,create=true;
inline unsigned creates=0,fallbacks=0;
inline std::atomic<unsigned> active{0};
inline std::vector<std::thread> threads;
inline std::mutex entryMutex;inline std::condition_variable entryCv;
inline bool holdEntry=false,entryWaiting=false;
inline void armEntry(){std::lock_guard<std::mutex> lock(entryMutex);holdEntry=true;entryWaiting=false;}
inline void waitEntry(){std::unique_lock<std::mutex> lock(entryMutex);assert(entryCv.wait_for(lock,std::chrono::seconds(3),[]{return entryWaiting;}));}
inline void releaseEntry(){std::lock_guard<std::mutex> lock(entryMutex);holdEntry=false;entryCv.notify_all();}
inline void join(){for(auto& t:threads)if(t.joinable())t.join();threads.clear();assert(!active);}
}
struct Debug {template<class... T>void printf(const char*,T...){++MockTask::fallbacks;}};
inline Debug Tab5DebugSerial;
inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn,const char* name,uint32_t stack,void* arg,unsigned priority,TaskHandle_t*,int core){
 assert(!strcmp(name,"tab5_catalogue")&&stack==8192&&priority==1&&core==0);
 ++MockTask::creates;if(!MockTask::create)return pdFAIL;
 ++MockTask::active;MockTask::threads.emplace_back([=]{
  {std::unique_lock<std::mutex> lock(MockTask::entryMutex);if(MockTask::holdEntry){MockTask::entryWaiting=true;MockTask::entryCv.notify_all();MockTask::entryCv.wait(lock,[]{return !MockTask::holdEntry;});}}
  fn(arg);--MockTask::active;
 });return pdPASS;
}
namespace Tab5Resources {
inline Reservation Manager::reserve(Owner owner,const Budget& budget,AdmissionReport*){
 assert(owner==Owner::Tools&&budget.internal==12288&&budget.internalBlock==8192);
 return MockTask::admit?Reservation(this,0):Reservation();
}
inline Reservation::~Reservation(){release();}
inline void Reservation::release(){_manager=nullptr;}
}
inline Tab5Resources::Manager Tab5MemoryManager(nullptr,nullptr);
