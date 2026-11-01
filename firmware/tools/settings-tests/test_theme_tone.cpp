#include "../../Tab5OS_Arduino_v0.8.18_Ecriture/Tab5ThemeTone.h"
#include <cassert>
#include <cstdio>
int main(){
 unsigned checked=0;
 for(unsigned value=0;value<256;++value){
  const uint32_t base=(value<<16)|((255-value)<<8)|((value*73)&255);
  assert(tab5theme::tone(base,0)==base);
  assert(tab5theme::tone(base,-100)==0);assert(tab5theme::tone(base,100)==0xffffff);
  assert(tab5theme::tone(base,-1000)==0);assert(tab5theme::tone(base,1000)==0xffffff);
  uint32_t previous=0;
  for(int amount=-100;amount<=100;++amount){
   const auto actual=tab5theme::tone(base,amount);
   for(unsigned shift=0;shift<=16;shift+=8){
    const int channel=(actual>>shift)&255;
    assert(channel>=int((previous>>shift)&255));
    const int mirror=(tab5theme::tone(base^0xffffff,-amount)>>shift)&255;
    assert(channel+mirror>=255&&channel+mirror<=256);
   }
   previous=actual;++checked;
  }
  // Each drag sample starts at the same base; returning to the centre
  // restores the original colour after visiting either endpoint.
  assert(tab5theme::tone(base,0)==base);
 }
 std::printf("PASS: %u tone samples, endpoints, centre, monotonic channels and black/white symmetry\n",checked);
}
