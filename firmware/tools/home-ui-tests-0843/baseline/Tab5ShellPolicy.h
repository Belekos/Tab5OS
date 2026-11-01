#pragma once
#include <cstdint>
#include <utility>

namespace Tab5Shell {
constexpr unsigned DockCount=7;
struct Choices {
  uint8_t version=1, enabled=1, visible=0x7f;
  uint8_t order[DockCount]={0,1,2,3,4,5,6};
  bool valid() const {
    if(version!=1||enabled>1||(visible&0x80))return false;
    unsigned seen=0;
    for(auto entry:order){if(entry>=DockCount||(seen&(1u<<entry)))return false;seen|=1u<<entry;}
    return seen==0x7f;
  }
  bool shown(unsigned item) const{return item<DockCount&&(visible&(1u<<item));}
  unsigned count() const{unsigned n=0;for(unsigned i=0;i<DockCount;++i)n+=shown(i);return n;}
  bool move(unsigned item,int direction){
    for(unsigned i=0;i<DockCount;++i)if(order[i]==item){int j=int(i)+(direction<0?-1:1);if(j<0||j>=int(DockCount))return false;std::swap(order[i],order[j]);return true;}
    return false;
  }
  int dockWidth() const{return enabled&&count()?64:0;}
};
// Exact integer anchors are evaluated from the current workspace width. No
// cumulative scaling, geometry inference, or changes to flex children.
inline int anchor(int workspace,int offset,unsigned divisor=1){return workspace/int(divisor)+offset;}
inline unsigned homeGroup(unsigned menu){return menu==1?2:menu==2?1:0;}
inline bool homeVisible(unsigned menu,unsigned kind,bool custom){return custom||homeGroup(menu)==kind;}
}
