#pragma once
#include <stdint.h>

namespace Tab5Appearance {
// Palette wire order is shared with the theme editor. The optional accent is
// derived, never written over the user's stored theme or foreground colours.
inline uint32_t mix(uint32_t a,uint32_t b,unsigned percent) {
  if(percent>100)percent=100;
  uint32_t result=0;
  for(unsigned shift=0;shift<=16;shift+=8)
    result|=((((a>>shift)&255)*(100-percent)+((b>>shift)&255)*percent+50)/100)<<shift;
  return result;
}
inline void palette(uint32_t* colors,bool colorful) {
  if(!colors||!colorful)return;
  const uint32_t bg=colors[0];
  const unsigned light=((bg>>16)&255)*299+((bg>>8)&255)*587+(bg&255)*114;
  const uint32_t accent=light<128000?0x559CDD:0x2868B4;
  colors[2]=mix(colors[2],accent,10);  // raised controls
  colors[3]=mix(colors[3],accent,28);  // card outlines
  colors[6]=mix(colors[6],accent,38);  // active controls
  colors[8]=mix(colors[8],accent,30);  // selection
}
}
