#pragma once
#include <stdint.h>
namespace tab5theme {
// A centred adjustment: 0 retains the selected colour; the two endpoints
// reach black and white. Every movement is evaluated from the same base.
inline uint32_t tone(uint32_t base,int amount){
 if(amount<-100)amount=-100;if(amount>100)amount=100;
 uint32_t result=0;const unsigned strength=unsigned(amount<0?-amount:amount);
 for(unsigned shift=0;shift<=16;shift+=8){
  const unsigned original=(base>>shift)&255;
  const unsigned target=amount<0?0:255;
  const unsigned channel=(original*(100-strength)+target*strength+50)/100;
  result|=channel<<shift;
 }
 return result;
}
}
