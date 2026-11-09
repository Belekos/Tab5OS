/* No AdLib/Sound Blaster is advertised. The PC speaker remains emulated. */
#include "pc_internal.h"
void sound_reset(void){}
bool sound_in(uint16_t p,uint8_t *v){(void)p;(void)v;return false;}
bool sound_out(uint16_t p,uint8_t v){(void)p;(void)v;return false;}
uint64_t sound_next_event(void){return NEVER;}
void sound_event(uint64_t n){(void)n;}
int32_t sound_sample(void){return 0;}
void sound_mix(int16_t *d,int n,uint32_t first){(void)d;(void)n;(void)first;}
