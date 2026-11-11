/* Exercise the delivered device adapters through guest-visible ports. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/ide.h"
#include "../core/misc.h"
#include "../core/i8257.h"
#include "../core/i8042.h"
#include "../core/vga.h"
#include "../storage.h"
static unsigned checks, errors, reads, writes, presents;
static uint32_t now;
static unsigned char disk[2880*512];
static int fail_read, fail_write;
#define CHECK(e) do { checks++; if (!(e)) { errors++; fprintf(stderr,"FAIL %d: %s\n",__LINE__,#e); } } while(0)
uint32_t get_uticks(void) { return now; }
int x86_disk_open(const char *path,int ro) { (void)ro; return !strcmp(path,"fd")?0:!strcmp(path,"ro")?1:-1; }
uint32_t x86_disk_sectors(int h) { return h>=0&&h<2?2880:0; }
int x86_disk_readonly(int h) { return h!=0; }
int x86_disk_read(int h,uint32_t lba,uint32_t count,uint8_t *out) {
 if(h<0||h>1||lba>2880||count>2880-lba||fail_read)return -1;
 reads++;memcpy(out,disk+lba*512,count*512);return 0;
}
int x86_disk_write(int h,uint32_t lba,uint32_t count,const uint8_t *in) {
 if(h!=0||lba>2880||count>2880-lba||fail_write)return -1;
 writes++;memcpy(disk+lba*512,in,count*512);return 0;
}
void x86_disk_close(int h) { (void)h; }
void x86_disk_geometry(int h,int *c,int *heads,int *s) { (void)h;*c=80;*heads=2;*s=18; }
static void irq(void *p,int n,int level) { (void)p;(void)n;(void)level; }
static void reset(void *p) { (void)p;errors++; }
static void draw(void *p,int x,int y,int w,int h) { (void)p;(void)x;(void)y;(void)w;(void)h;presents++; }
static void floppy_cmd(EMULINK *e,unsigned cmd,unsigned drive,unsigned chs,unsigned n) {
 emulink_cmd_write(e,cmd);emulink_data_write(e,drive);emulink_data_write(e,chs);emulink_data_write(e,n);
}
static void floppy_test(void) {
 EMULINK *e=emulink_init();uint8_t got[1024],sent[1024];
 for(unsigned i=0;i<sizeof(sent);i++)sent[i]=(uint8_t)(i*7+11);
 CHECK(!emulink_attach_floppy(e,0,"fd"));CHECK(!emulink_attach_floppy(e,1,"ro"));
 CHECK(emulink_attach_floppy(e,2,"fd")<0);CHECK(emulink_attach_floppy(e,-1,"fd")<0);
 floppy_cmd(e,0x102,0,1,2);CHECK(!emulink_status_read(e));
 CHECK(emulink_data_write_string(e,sent,1,17)==17);
 CHECK(emulink_data_write_string(e,sent+17,1,1007)==1007);
 CHECK(!emulink_status_read(e));CHECK(!memcmp(disk,sent,1024));
 floppy_cmd(e,0x101,0,1,2);memset(got,0,sizeof(got));
 emulink_data_read_string(e,got,1,509);emulink_data_read_string(e,got+509,1,515);
 CHECK(!memcmp(got,sent,1024));CHECK(!emulink_status_read(e));
 unsigned before=writes;floppy_cmd(e,0x102,1,1,1);CHECK(emulink_status_read(e)!=0);CHECK(writes==before);
 floppy_cmd(e,0x101,2,1,1);CHECK(emulink_status_read(e)!=0);
 floppy_cmd(e,0x101,0,0,1);CHECK(emulink_status_read(e)!=0);
 floppy_cmd(e,0x101,0,(79u<<16)|(1u<<8)|18u,2);CHECK(emulink_status_read(e)!=0);
 floppy_cmd(e,0x101,0,1,UINT32_MAX);CHECK(emulink_status_read(e)!=0);
 fail_read=1;floppy_cmd(e,0x101,0,1,1);emulink_data_read_string(e,got,2,256);CHECK(emulink_status_read(e)!=0);fail_read=0;
 fail_write=1;floppy_cmd(e,0x102,0,1,1);emulink_data_write_string(e,sent,2,256);CHECK(emulink_status_read(e)!=0);fail_write=0;
 floppy_cmd(e,0x101,0,1,1);emulink_data_read_string(e,got,4,INT32_MAX);CHECK(emulink_status_read(e)!=0);
}
static void ide_command(IDEIFState *s,unsigned lba,unsigned cmd) {
 ide_ioport_write(s,6,0xe0|(lba>>24));ide_ioport_write(s,2,1);
 ide_ioport_write(s,3,lba&255);ide_ioport_write(s,4,(lba>>8)&255);ide_ioport_write(s,5,(lba>>16)&255);
 ide_ioport_write(s,7,cmd);
}
static void ide_test(void) {
 IDEIFState *s=ide_allocate(14,NULL,irq);CHECK(ide_attach(s,0,"missing")<0);CHECK(!ide_attach(s,0,"ro"));
 ide_command(s,0,0x20);CHECK((ide_status_read(s)&9)==8);
 for(unsigned i=0;i<256;i++)CHECK(ide_data_readw(s)==(unsigned)(disk[i*2]|disk[i*2+1]<<8));
 ide_command(s,2880,0x20);CHECK(ide_status_read(s)&1);
 unsigned before=writes;ide_command(s,0,0x30);for(unsigned i=0;i<256;i++)ide_data_writew(s,0x5555);
 CHECK(ide_status_read(s)&1);CHECK(writes==before);
 fail_read=1;ide_command(s,0,0x20);CHECK(ide_status_read(s)&1);fail_read=0;
}
static void dma_test(void) {
 unsigned char guarded[34],out[8],in[8]={1,2,3,4,5,6,7,8};memset(guarded,0xa5,sizeof(guarded));
 I8257State *d=i8257_new((char*)guarded+1,32,0,0x80,0x480,0);d->regs[0].mode=4;d->regs[0].now[0]=30;
 CHECK(i8257_dma_write_memory(d,0,in,0,8)==8);CHECK(guarded[31]==1&&guarded[32]==2&&guarded[33]==0xa5);
 CHECK(i8257_dma_read_memory(d,0,out,0,8)==8);CHECK(out[0]==1&&out[1]==2&&out[2]==255&&out[7]==255);
 CHECK(i8257_dma_read_memory(d,0,out,-1,8)==0);CHECK(i8257_dma_write_memory(d,0,in,0,-1)==0);
 d->regs[0].mode=0x24;d->regs[0].now[0]=1;
 i8257_dma_write_memory(d,0,in,0,8);CHECK(guarded[2]==1&&guarded[1]==2&&guarded[0]==0xa5);
 i8257_dma_read_memory(d,0,out,0,8);CHECK(out[0]==1&&out[1]==2&&out[2]==255);
 d->regs[0].pageh=127;d->regs[0].page=255;d->regs[0].now[0]=65535;d->regs[0].mode=4;
 i8257_dma_read_memory(d,0,out,INT32_MAX,8);CHECK(out[0]==255&&out[7]==255);
 CHECK(guarded[0]==0xa5&&guarded[33]==0xa5);
}
static void input_test(void) {
 PS2KbdState*k;PS2MouseState*m;KBDState*s=i8042_init(&k,&m,1,12,NULL,irq,NULL,reset);
 ps2_keyboard_set_translation(k,1); /* BIOS normally enables Set-1 translation. */
 CHECK(!ps2_keyboard_pending(k));ps2_put_keycode(k,1,30);CHECK(ps2_keyboard_pending(k));CHECK(kbd_read_data(s,0)==30);CHECK(!ps2_keyboard_pending(k));
 ps2_put_keycode(k,0,30);CHECK(kbd_read_data(s,0)==158);
 ps2_put_keycode(k,1,0xe04d);CHECK(kbd_read_data(s,0)==0xe0);CHECK(ps2_keyboard_pending(k));now+=10001;kbd_step(s);CHECK(kbd_read_data(s,0)==0x4d);CHECK(!ps2_keyboard_pending(k));
 ps2_write_mouse(m,0xf4);CHECK(ps2_read_data(m)==0xfa);now+=20000;ps2_mouse_event(m,7,-4,0,1);
 unsigned a=ps2_read_data(m),b=ps2_read_data(m),c=ps2_read_data(m);CHECK((a&9)==9&&b==7&&c==4);
}
static void vbe_reg(VGAState*s,int r,int v) { vbe_write(s,0,r);vbe_write(s,1,v); }
static void video_test(void) {
 const size_t size=1024u*768u*2u;uint8_t *guarded=malloc(size+32),*ram=calloc(1,1024u*1024u);memset(guarded,0xa7,size+32);
 VGAState*s=vga_init((char*)ram,1024*1024,guarded+16,1024,768);int x,y,w,h,t;
 vga_refresh(s,draw,NULL,1);vga_view(s,&x,&y,&w,&h,&t);CHECK(t&&w==720&&h==400&&x==152&&y==184);
 vbe_reg(s,1,800);vbe_reg(s,2,600);vbe_reg(s,3,8);vbe_reg(s,4,1);vga_refresh(s,draw,NULL,1);
 vga_view(s,&x,&y,&w,&h,&t);CHECK(!t&&w==800&&h==600&&x==112&&y==84);
 vbe_reg(s,1,1024);vbe_reg(s,2,768);vga_refresh(s,draw,NULL,1);vga_view(s,&x,&y,&w,&h,&t);CHECK(w==1024&&h==768&&x==0&&y==0);
 vbe_reg(s,8,65535);vbe_reg(s,9,65535);vga_refresh(s,draw,NULL,1);
 for(int i=0;i<16;i++)CHECK(guarded[i]==0xa7&&guarded[size+16+i]==0xa7);
 CHECK(presents>=4);
}
int main(void) { floppy_test();ide_test();dma_test();input_test();video_test();printf("DOS x86 devices: %u checks, %u failures\n",checks,errors);return errors?1:0; }
