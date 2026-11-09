#pragma once
#include "Tab5AppApi.h"
// The owner serializes push/pop. Bounded storage belongs to the canvas PSRAM
// allocation and is freed with it; no application pointer or callback survives.
class Tab5AppInputQueue {
 public:
  void push(const tab5_app_input_event& event) {
    if (event.type == TAB5_INPUT_MOVE && count_ &&
        events_[(head_ + count_ - 1) % Capacity].type == TAB5_INPUT_MOVE &&
        events_[(head_ + count_ - 1) % Capacity].modifiers == event.modifiers) {
      events_[(head_ + count_ - 1) % Capacity] = event;
      return;
    }
    if (count_ == Capacity) {
      head_ = count_ = 0;
      tab5_app_input_event cancel{};
      cancel.size = sizeof(cancel); cancel.type = TAB5_INPUT_CANCEL;
      events_[count_++] = cancel;
      if (event.type == TAB5_INPUT_MOVE || event.type == TAB5_INPUT_UP) return;
    }
    events_[(head_ + count_) % Capacity] = event;
    ++count_;
  }
  void pushPointer(const tab5_app_input_event& position,const tab5_app_input_event& relative) {
    // Coalesce motion only while buttons are unchanged. Every press/release
    // survives; summing relative reports avoids losing motion near screen edges.
    if(count_>=2){
      auto& p=events_[(head_+count_-2)%Capacity];
      auto& r=events_[(head_+count_-1)%Capacity];
      if(p.type==TAB5_INPUT_POINTER_POSITION&&r.type==TAB5_INPUT_POINTER_RELATIVE&&
         p.key==position.key&&r.key==relative.key){
        p=position;r.x=addMotion(r.x,relative.x);r.y=addMotion(r.y,relative.y);return;
      }
    }
    if(count_>Capacity-2){head_=count_=0;tab5_app_input_event cancel{};
      cancel.size=sizeof(cancel);cancel.type=TAB5_INPUT_CANCEL;events_[count_++]=cancel;}
    push(position);push(relative);
  }
  bool pop(tab5_app_input_event& event) {
    if (!count_) return false;
    event = events_[head_]; head_ = (head_ + 1) % Capacity; --count_;
    return true;
  }
 private:
  static int32_t addMotion(int32_t a,int32_t b){
    const int64_t total=int64_t(a)+b;return total>32767?32767:total< -32767?-32767:int32_t(total);
  }
  static constexpr unsigned Capacity = 32;
  tab5_app_input_event events_[Capacity]{};
  unsigned head_ = 0, count_ = 0;
};
