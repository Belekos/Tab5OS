#pragma once
#include <cstdint>
#include <algorithm>
#include <utility>
#include "Tab5AppCataloguePolicy.h"

namespace Tab5Shell {
constexpr unsigned LegacyDockCount=7;
constexpr unsigned DockButtonHeight=56,DockGap=6,DockMargin=10;
constexpr unsigned DockCount=(672-DockMargin*2+DockGap)/(DockButtonHeight+DockGap);
inline unsigned capacity(int height){return height<int(DockMargin*2+DockButtonHeight)?0:std::min<unsigned>(DockCount,(unsigned(height)-DockMargin*2+DockGap)/(DockButtonHeight+DockGap));}
inline uint64_t homeKey(){return Tab5AppCatalogue::keyFor("home");}
inline uint64_t legacyKey(unsigned index){const char* ids[]={"home","files","editor","reader","audio","map","settings"};return index<LegacyDockCount?Tab5AppCatalogue::keyFor(ids[index]):0;}
struct LegacyChoices {
  uint8_t version=1, enabled=1, visible=0x7f;
  uint8_t order[LegacyDockCount]={0,1,2,3,4,5,6};
  bool valid() const {
    if(version!=1||enabled>1||(visible&0x80))return false;
    unsigned seen=0;
    for(auto entry:order){if(entry>=LegacyDockCount||(seen&(1u<<entry)))return false;seen|=1u<<entry;}
    return seen==0x7f;
  }
};
static_assert(sizeof(LegacyChoices)==10,"sidebar v1 on-disk layout");
struct Choices {
  uint32_t version=2;
  uint8_t enabled=1,count=LegacyDockCount;
  uint16_t reserved=0;
  uint64_t keys[DockCount]{};
  Choices(){for(unsigned i=0;i<LegacyDockCount;++i)keys[i]=legacyKey(i);}
  bool valid() const{
    if(version!=2||enabled>1||count>DockCount||reserved)return false;
    for(unsigned i=0;i<DockCount;++i){
      if(i>=count){if(keys[i])return false;continue;}
      if(!keys[i])return false;
      for(unsigned j=0;j<i;++j)if(keys[i]==keys[j])return false;
    }
    return true;
  }
  bool contains(uint64_t key) const{for(unsigned i=0;i<count;++i)if(keys[i]==key)return true;return false;}
  bool add(uint64_t key,unsigned limit){if(!key||count>=std::min(limit,DockCount)||contains(key))return false;keys[count++]=key;return true;}
  bool remove(unsigned slot){if(slot>=count)return false;for(unsigned i=slot+1;i<count;++i)keys[i-1]=keys[i];keys[--count]=0;return true;}
  bool move(unsigned slot,int direction){
    const int next=int(slot)+(direction<0?-1:1);
    if(slot>=count||next<0||next>=int(count))return false;
    std::swap(keys[slot],keys[next]);return true;
  }
  bool migrate(const LegacyChoices& old){
    if(!old.valid())return false;
    Choices next;next.enabled=old.enabled;next.count=0;for(auto& key:next.keys)key=0;
    for(auto index:old.order)if(old.visible&(1u<<index))next.keys[next.count++]=legacyKey(index);
    *this=next;return true;
  }
  int dockWidth() const{return enabled&&count?64:0;}
};
static_assert(sizeof(Choices)==88,"sidebar v2 on-disk layout");
// Exact integer anchors are evaluated from the current workspace width. No
// cumulative scaling, geometry inference, or changes to flex children.
inline int anchor(int workspace,int offset,unsigned divisor=1){return workspace/int(divisor)+offset;}
inline unsigned homeGroup(unsigned menu){return menu==1?2:menu==2?1:0;}
inline bool homeVisible(unsigned menu,unsigned kind,bool custom){return custom||homeGroup(menu)==kind;}
}
