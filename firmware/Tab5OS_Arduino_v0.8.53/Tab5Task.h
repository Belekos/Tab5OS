#pragma once
#include "Tab5Serial.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Tab5ResourceManager.h"
#ifdef ARDUINO
#include <Arduino.h>
#include <new>
#include "Tab5Memory.h"
#endif

// Call on the worker itself after its real workload, before self-deletion.
// ESP-IDF reports bytes, unlike the upstream FreeRTOS word-based API.
inline void tab5TaskCheckpoint(const char* name) {
#ifdef ARDUINO
  const unsigned remaining=unsigned(uxTaskGetStackHighWaterMark(nullptr));
  Tab5DebugSerial.printf("[STACK] %s: marge minimale=%u octets%s\n",name,remaining,
      remaining<2048?" — pile a augmenter":"");
#else
  (void)name;
#endif
}

#ifdef ARDUINO
namespace tab5task {
struct Start {
  TaskFunction_t function;
  void* argument;
  const char* name;
  std::atomic<bool> ready{false};
  Start(TaskFunction_t f,void* a,const char* n):function(f),argument(a),name(n){}
};
inline void enter(void* argument) {
  auto* start=static_cast<Start*>(argument);
  // xTaskCreate may run us on the other core before it returns. The real task
  // cannot request a second budget until its stack reservation is released.
  while(!start->ready.load(std::memory_order_acquire))vTaskDelay(1);
  auto function=start->function;void* original=start->argument;
  tab5TaskStarted(start->name);
  delete start;
  function(original);
  // Native jobs normally delete themselves; tolerate jobs that simply return.
  vTaskDelete(nullptr);
}
}
#endif

// Existing admissions include their own stack and working set. All predicted
// startup allocations must have materialised before handing control to the job.
// This variant avoids charging a second TaskBudget for Python/maps/video.
template<class Function,class Handle>
auto tab5CreateAdmittedTask(Tab5Resources::Reservation& admission,
    Function function,const char* name,uint32_t stack,
    void* argument,unsigned priority,Handle handle,int core)
    -> decltype(xTaskCreatePinnedToCore(function,name,stack,argument,priority,handle,core)) {
#ifdef ARDUINO
  if(!admission)return pdFAIL;
  auto* start=new(std::nothrow) tab5task::Start(function,argument,name);
  if(!start)return pdFAIL;
  const auto result=xTaskCreatePinnedToCore(tab5task::enter,name,stack,start,priority,handle,core);
  // The stack/TCB are now in the live heap sample. The worker cannot have
  // consumed or requested any further resources yet.
  admission.release();
  if(result==pdPASS)start->ready.store(true,std::memory_order_release);
  else delete start;
  return result;
#else
  (void)admission;
  return xTaskCreatePinnedToCore(function,name,stack,argument,priority,handle,core);
#endif
}

// Simple native jobs admit only their stack, then hand off to the job.
template<class Function,class Handle>
auto tab5CreateTask(Function function,const char* name,uint32_t stack,
    void* argument,unsigned priority,Handle handle,int core,
    Tab5Resources::AdmissionReport* report=nullptr)
    -> decltype(xTaskCreatePinnedToCore(function,name,stack,argument,priority,handle,core)) {
#ifdef ARDUINO
  auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,
      Tab5Resources::TaskBudget(stack),name,report);
  return tab5CreateAdmittedTask(admission,function,name,stack,argument,priority,handle,core);
#else
  (void)report;
  return xTaskCreatePinnedToCore(function,name,stack,argument,priority,handle,core);
#endif
}
