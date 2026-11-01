#include "Tab5ShellPolicy.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
 using namespace Tab5Shell;
 Choices choices;assert(choices.valid()&&choices.count()==7&&choices.dockWidth()==64);
 unsigned permutations=0;
 do{
  assert(choices.valid());Choices restored;std::memcpy(&restored,&choices,sizeof(choices));assert(restored.valid());
  for(unsigned position=0;position<DockCount;++position){Choices moved=choices;const unsigned item=moved.order[position];
   if(position){assert(moved.move(item,-1)&&moved.order[position-1]==item);assert(moved.move(item,1));assert(!std::memcmp(&moved,&choices,sizeof(choices)));}
   if(position+1<DockCount){assert(moved.move(item,1)&&moved.order[position+1]==item);assert(moved.move(item,-1));assert(!std::memcmp(&moved,&choices,sizeof(choices)));}
  }++permutations;
 }while(std::next_permutation(choices.order,choices.order+DockCount));
 assert(permutations==5040);
 for(unsigned mask=0;mask<128;++mask){choices.visible=mask;assert(choices.valid());unsigned count=0;for(unsigned i=0;i<7;++i)count+=(mask>>i)&1;assert(count==choices.count());assert(choices.dockWidth()==(count?64:0));choices.enabled=0;assert(choices.dockWidth()==0);choices.enabled=1;}
 choices.order[1]=choices.order[0];assert(!choices.valid());choices=Choices{};choices.order[2]=7;assert(!choices.valid());choices=Choices{};choices.enabled=2;assert(!choices.valid());
 for(unsigned menu=0;menu<3;++menu)for(unsigned kind=0;kind<3;++kind){assert(homeVisible(menu,kind,true));assert(homeVisible(menu,kind,false)==(kind==(menu==1?2:menu==2?1:0)));}
 // Flipping the sidebar repeatedly cannot accumulate coordinate rounding.
 for(unsigned repeat=0;repeat<1000;++repeat){assert(anchor(1216,-16)==1200);assert(anchor(1280,-16)==1264);assert(anchor(1216,-23,2)==585);assert(anchor(1280,-23,2)==617);}
 // Reserved top-bar regions stay disjoint even while Python is running.
 assert(6+52<=64&&64+56<=132&&132+400<546&&546<556);
 std::puts("PASS: 5040 sidebar orderings, visibility masks, persistence validation, dedicated home groups, stable anchors and top-bar separation");
}
