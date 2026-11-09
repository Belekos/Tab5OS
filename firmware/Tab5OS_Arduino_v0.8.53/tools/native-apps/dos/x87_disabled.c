/* Tab5OS DOS has no 8087/80287 coprocessor. The CPU's hardware-absent
 * paths remain active; these symbols are never executed with fpu=false. */
#include "x87.h"
#include <string.h>
void x87_reset(x87_t *f, bool i8087) { memset(f, 0, sizeof(*f)); f->i8087=i8087; f->cw=0x37f; f->tw=0xffff; }
void x87_init(x87_t *f) { bool old=f->i8087; x87_reset(f,old); }
void x87_operand(uint8_t op,uint8_t reg,int *read,int *write) { (void)op;(void)reg;*read=*write=0; }
int x87_exec(x86_t *c,uint8_t op,uint8_t mod,uint8_t reg,uint8_t rm,uint8_t *mem,uint16_t off,uint16_t seg) {
 (void)c;(void)op;(void)mod;(void)reg;(void)rm;(void)mem;(void)off;(void)seg;return 1;
}
