#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Tab5AppCatalogue {
constexpr unsigned MaxApps = 64;
constexpr unsigned SymbolCount = 25; // Automatic, then twenty-four built-in symbols.
inline uint64_t keyFor(const char* id) {
  uint64_t key=14695981039346656037ULL;
  for(const unsigned char* p=reinterpret_cast<const unsigned char*>(id);p&&*p;++p)
    key=(key^*p)*1099511628211ULL;
  return key ? key : 1;
}
struct Choice { uint64_t key=0; uint8_t icon=0, pinned=0; uint16_t order=0; uint32_t reserved=0; };
struct Choices {
  uint32_t version=2;
  uint8_t personal=0, count=0;
  uint16_t display=0; // bit 0: list, bits 1-2: density, bit 3: hide labels
  bool list() const { return display&1; }
  unsigned density() const { return (display>>1)&3; }
  bool labels() const { return !(display&8); }
  void setList(bool value) { display=(display&~1U)|(value?1U:0U); }
  void setDensity(unsigned value) { display=(display&~6U)|((value<3?value:0U)<<1); }
  void setLabels(bool value) { display=(display&~8U)|(value?0U:8U); }
  Choice apps[MaxApps]{};
  bool valid() const {
    if(version!=2||personal>1||count>MaxApps||(display&~15U)||density()>2)return false;
    for(unsigned i=0;i<count;++i){
      if(!apps[i].key||apps[i].icon>=SymbolCount||apps[i].pinned>1||apps[i].order>=MaxApps)return false;
      for(unsigned j=0;j<i;++j)if(apps[j].key==apps[i].key)return false;
    }
    return true;
  }
  bool upgrade() { if(version==1){version=2;display=0;} return valid(); }
  int find(uint64_t key) const {for(unsigned i=0;i<count;++i)if(apps[i].key==key)return int(i);return -1;}
  Choice* get(uint64_t key,bool builtin) {
    const int index=find(key);if(index>=0)return &apps[index];
    if(count==MaxApps)return nullptr;
    Choice& c=apps[count];c.key=key;c.pinned=builtin;c.order=count++;return &c;
  }
};
static_assert(sizeof(Choices)==1032,"home preferences keep the 0.8.16 binary size");
}
