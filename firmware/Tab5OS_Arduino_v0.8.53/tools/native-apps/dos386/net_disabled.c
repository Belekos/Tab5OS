#include "core/ne2000.h"
void ne2000_ioport_write(void *p,uint32_t a,uint32_t v){(void)p;(void)a;(void)v;}
uint32_t ne2000_ioport_read(void *p,uint32_t a){(void)p;(void)a;return 255;}
void ne2000_reset_ioport_write(void *p,uint32_t a,uint32_t v){(void)p;(void)a;(void)v;}
uint32_t ne2000_reset_ioport_read(void *p,uint32_t a){(void)p;(void)a;return 255;}
void ne2000_asic_ioport_write(void *p,uint32_t a,uint32_t v){(void)p;(void)a;(void)v;}
uint32_t ne2000_asic_ioport_read(void *p,uint32_t a){(void)p;(void)a;return 255;}
void ne2000_step(NE2000State *p){(void)p;}
NE2000State *isa_ne2000_init(int b,int i,void *p,void(*f)(void*,int,int)){(void)b;(void)i;(void)p;(void)f;return 0;}
void ne2000_set_config_file(const char *p){(void)p;}
