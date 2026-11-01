#pragma once
#include "FreeRTOS.h"
#include <mutex>
#include <thread>
#include <cassert>
#include <atomic>
struct StaticSemaphore_t{std::timed_mutex mutex;std::thread::id owner;};
using SemaphoreHandle_t=StaticSemaphore_t*;
extern std::atomic<unsigned> takes,gives,mutexCreates;
inline auto xSemaphoreCreateMutexStatic(StaticSemaphore_t*p){++mutexCreates;return p;}
inline int xSemaphoreTake(SemaphoreHandle_t p,TickType_t wait){++takes;bool result=wait?p->mutex.try_lock_for(std::chrono::milliseconds(wait)):p->mutex.try_lock();if(!result)return pdFALSE;p->owner=std::this_thread::get_id();return pdTRUE;}
inline int xSemaphoreGive(SemaphoreHandle_t p){assert(p->owner==std::this_thread::get_id());p->owner={};++gives;p->mutex.unlock();return pdTRUE;}
