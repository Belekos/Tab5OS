#pragma once
#include <cstddef>
#include <cstdint>
namespace writing {
struct HidKeys {
  uint8_t modifiers = 0;
  uint8_t bits[32]{};
  bool down(unsigned code) const { return code < 256 && (bits[code / 8] & (1u << (code % 8))); }
  void set(unsigned code) { if (code < 256) bits[code / 8] |= 1u << (code % 8); }
};
class HidKeyboardMux {
 public:
  static constexpr unsigned Capacity=8;
  HidKeys update(unsigned slot,const HidKeys& keys,bool disconnected=false){
    if(slot<Capacity)held_[slot]=disconnected?HidKeys{}:keys;
    HidKeys merged{};for(const auto& h:held_){merged.modifiers|=h.modifiers;for(unsigned b=0;b<32;++b)merged.bits[b]|=h.bits[b];}return merged;
  }
  void clear(){for(auto& h:held_)h={};}
 private:HidKeys held_[Capacity];
};
// Fixed bounds: descriptors and reports never allocate from untrusted USB sizes.
class HidKeyboardDescriptor {
 public:
  bool parse(const uint8_t* descriptor, size_t size);
  bool decode(const uint8_t* report, size_t size, HidKeys& keys) const;
  static bool boot(const uint8_t* report, size_t size, HidKeys& keys);
 private:
  struct Field { uint16_t offset, count, usage, maximum; uint8_t size, id; bool variable; };
  Field fields_[256]{};
  uint16_t count_ = 0, lengths_[256]{};
  bool ids_ = false;
  uint8_t slots_[256]{};uint8_t slotCount_=0;
  mutable HidKeys reports_[16]{};
};
}
