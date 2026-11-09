#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_internal.h"
uint8_t *vga_planes(void);
static int checks,failures;
static void check(int ok,const char* message){checks++;if(!ok){fprintf(stderr,"FAIL: %s\n",message);failures++;}}
static void frames(unsigned count){while(count--)pc_run_frame();}
static int contains(const char *word){
 uint8_t *v=vga_planes();size_t len=strlen(word);
 for(unsigned i=0;i+len<2000;i++){size_t j=0;while(j<len&&v[(i+j)*8]==(unsigned char)word[j])j++;if(j==len)return 1;}return 0;
}
/* Call the actual resident INT33 handler at an instruction boundary. Registers
 * and time resume exactly where the paused guest was; the driver's own memory
 * and I/O side effects remain real. This is a host fixture, never shipped code. */
static int mouse_call(uint16_t ax,uint16_t bx,uint16_t cx,uint16_t dx,uint16_t es,uint16_t out[4]){
 x86_t saved=pc.cpu;pc.cpu.r.w[X86_AX]=ax;pc.cpu.r.w[X86_BX]=bx;pc.cpu.r.w[X86_CX]=cx;pc.cpu.r.w[X86_DX]=dx;
 x86_load_seg(&pc.cpu,X86_ES,es);x86_interrupt(&pc.cpu,0x33);
 int done=0;
 for(unsigned n=0;n<100000;n++){
  if(pc.cpu.seg[X86_CS].sel==saved.seg[X86_CS].sel&&pc.cpu.ip==saved.ip&&pc.cpu.r.w[X86_SP]==saved.r.w[X86_SP]){done=1;break;}
  x86_step(&pc.cpu);
 }
 out[0]=pc.cpu.r.w[X86_AX];out[1]=pc.cpu.r.w[X86_BX];out[2]=pc.cpu.r.w[X86_CX];out[3]=pc.cpu.r.w[X86_DX];
 fprintf(stderr,"INT33 %04x -> %04x %04x %04x %04x done=%d\n",ax,out[0],out[1],out[2],out[3],done);
 pc.cpu=saved;return done;
}
int main(int argc,char **argv){
 if(argc!=3)return 2;pc_config_t cfg={0};cfg.board=PC_AT;cfg.cpu=X86_80286;cfg.cpu_hz=6000000;cfg.video=PC_VGA;cfg.conv_kb=640;cfg.ext_kb=1024;
 if(posix_memalign((void**)&cfg.ram,4096,PC_RAM_BYTES(cfg.ext_kb)))return 3;cfg.page_table=calloc(X86_PAGES,sizeof(uintptr_t));
 if(!cfg.page_table||!pc_init(&cfg)||!pc_floppy_insert(0,argv[1])||!pc_floppy_insert(1,argv[2]))return 4;
 pc_reset();for(unsigned n=0;n<1000&&!contains("TAB5OS DOS READY");n++)pc_run_frame();
 check(contains("TAB5OS DOS READY"),"original FreeDOS boot image remains bootable");
 pc_type("b:\\ctmouse /s14 /r11\n");frames(500);
 unsigned vector=0x33*4;check(pc.ram[vector]|pc.ram[vector+1]|pc.ram[vector+2]|pc.ram[vector+3],"CuteMouse installs INT33 vector");
 uint16_t r[4];check(mouse_call(0,0,0,0,0,r)&&r[0]==0xffff&&r[1]>=2,"INT33 reset finds real two-button driver");
 check(mouse_call(4,0,300,100,0,r),"position can be set through INT33");
 check(mouse_call(3,0,0,0,0,r)&&r[2]==296&&r[3]==96,"INT33 text-mode position snaps to the containing 8x8 character cell");
 check(mouse_call(0x0b,0,0,0,0,r),"clear motion counters");
 pc_mouse(24,12,1);frames(12);
 check(mouse_call(3,0,0,0,0,r)&&r[1]==1&&r[2]>300&&r[3]>100,"serial IRQ4 delivers motion and left button");
 check(mouse_call(0x0b,0,0,0,0,r)&&(int16_t)r[2]>0&&(int16_t)r[3]>0,"INT33 reports positive movement after the driver's documented sensitivity/acceleration");
 pc_mouse(-10,-6,2);frames(12);
 check(mouse_call(3,0,0,0,0,r)&&r[1]==2,"serial right button reaches guest");
 check(mouse_call(0x0b,0,0,0,0,r)&&(int16_t)r[2]<0&&(int16_t)r[3]<0,"negative serial motion is signed correctly after driver sensitivity");
 pc_mouse(0,0,0);frames(12);check(mouse_call(3,0,0,0,0,r)&&r[1]==0,"all buttons release");
 /* A very short click between frames must survive the UART's edge retention. */
 mouse_call(5,0,0,0,0,r);pc_mouse(0,0,1);pc_mouse(0,0,0);frames(18);
 check(mouse_call(5,0,0,0,0,r)&&r[1]>0,"short touch tap is visible as guest button press");
 check(mouse_call(3,0,0,0,0,r)&&r[1]==0,"short tap also releases after delivery");
 /* Driver-driven far callback (INC word CS:[0100]; RETF) on movement/buttons. */
 static const uint8_t callback[]={0x2e,0xff,0x06,0x00,0x01,0xcb};memcpy(pc.ram+0x90000,callback,sizeof(callback));pc.ram[0x90100]=pc.ram[0x90101]=0;
 check(mouse_call(0x0c,0,0x1f,0,0x9000,r),"register INT33 application callback");pc_mouse(4,3,1);frames(18);
 check(pc.ram[0x90100]|pc.ram[0x90101],"real driver invokes application callback from mouse IRQ");
 mouse_call(0x0c,0,0,0,0,r);pc_mouse(0,0,0);frames(12);
 check(!pc_halted(),"guest remains running after motion, clicks and callback");
 printf("{\"passed\":%s,\"checks\":%d,\"failures\":%d,\"component\":\"Real FreeDOS/CuteMouse serial IRQ4 and INT33\"}\n",failures?"false":"true",checks,failures);
 return failures?1:0;
}
