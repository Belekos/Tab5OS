#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Call update/snapshot under the same short critical section. Keeping physical
// keyboards separate makes disconnect and overlapping held keys unambiguous.
template<size_t Capacity> class Tab5KeyboardState {
 struct Device { uint8_t keys[32]{},modifiers=0;bool connected=false; } devices_[Capacity];
 public:
 void update(size_t slot,const uint8_t* keys,uint8_t modifiers,bool connected){
  if(slot>=Capacity)return;
  auto& d=devices_[slot];d.connected=connected;d.modifiers=connected?modifiers:0;
  if(connected&&keys)memcpy(d.keys,keys,sizeof(d.keys));else memset(d.keys,0,sizeof(d.keys));
 }
 bool snapshot(bool active,uint8_t* keys,uint8_t& modifiers)const{
  memset(keys,0,32);modifiers=0;if(!active)return false;
  bool connected=false;
  for(const auto& d:devices_)if(d.connected){
   connected=true;modifiers|=d.modifiers;
   for(size_t i=0;i<32;++i)keys[i]|=d.keys[i];
  }
  return connected;
 }
};
