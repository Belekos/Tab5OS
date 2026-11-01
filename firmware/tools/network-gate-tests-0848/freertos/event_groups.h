#pragma once
#include <mutex>
#include <condition_variable>
#include <freertos/FreeRTOS.h>
using EventBits_t=uint32_t;
struct StaticEventGroup_t{std::mutex mutex;std::condition_variable changed;EventBits_t bits=0;};
using EventGroupHandle_t=StaticEventGroup_t*;
inline auto xEventGroupCreateStatic(StaticEventGroup_t*p){return p;}
inline EventBits_t xEventGroupSetBits(EventGroupHandle_t p,EventBits_t bits){std::lock_guard<std::mutex> l(p->mutex);p->bits|=bits;p->changed.notify_all();return p->bits;}
inline EventBits_t xEventGroupClearBits(EventGroupHandle_t p,EventBits_t bits){std::lock_guard<std::mutex> l(p->mutex);auto old=p->bits;p->bits&=~bits;return old;}
inline EventBits_t xEventGroupWaitBits(EventGroupHandle_t p,EventBits_t bits,int clear,int all,TickType_t wait){std::unique_lock<std::mutex> l(p->mutex);const auto ready=[&]{return all?(p->bits&bits)==bits:(p->bits&bits)!=0;};if(!ready()&&wait)p->changed.wait_for(l,std::chrono::milliseconds(wait),ready);auto result=p->bits;if(clear&&ready())p->bits&=~bits;return result;}
