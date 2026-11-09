#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_internal.h"
uint8_t *vga_planes(void);
int dos_folder_name(struct folder *,const char *,char[13]);
static uint32_t keyboard_vectors(void){const unsigned vector[]={9,0x15,0x16,0x2f};uint32_t h=2166136261u;for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)h=(h^pc.ram[vector[i]*4+j])*16777619u;return h;}
static void dump_screen(void) {
 for(int y=0;y<25;y++) {
  char line[81];for(int x=0;x<80;x++){unsigned char c=vga_planes()[(y*80+x)*8];line[x]=c>=32&&c<127?c:' ';}
  line[80]=0;puts(line);
 }
}
int main(int argc,char **argv) {
 if(argc<2){fprintf(stderr,"boot_host FLOPPY [PROGRAM_FOLDER]\n");return 2;}
 pc_config_t cfg={0};cfg.board=PC_AT;cfg.cpu=X86_80286;cfg.cpu_hz=6000000;cfg.video=PC_VGA;
 cfg.conv_kb=640;cfg.ext_kb=1024;cfg.fpu=false;
 if(posix_memalign((void**)&cfg.ram,4096,PC_RAM_BYTES(cfg.ext_kb)))return 3;
 cfg.page_table=calloc(X86_PAGES,sizeof(uintptr_t));
 if(!cfg.page_table||!pc_init(&cfg)||!pc_floppy_insert(0,argv[1]))return 4;
 if(argc>2&&!pc_hd_insert(0,argv[2]))return 5;
 pc_reset();
 uint32_t initial_irq9=0;int french_ok=0;
 char launch[80]="ver\n";
 if(argc>2){char alias[13];if(!dos_folder_name(disks[2].folder,argc>3?argv[3]:"HELLO.EXE",alias))return 7;snprintf(launch,sizeof(launch),"c:\nhello.com\n%s\n",alias);fprintf(stderr,"Executable alias: %s\n",alias);}
 for(int frame=0;frame<1700;frame++) {
  if(frame==400)pc_type(launch);
  if(frame==800){dump_screen();initial_irq9=keyboard_vectors();pc_type("keyb fr,437,a:\\keyboard.sys\n");}
  if(frame==1050){french_ok=keyboard_vectors()!=initial_irq9;if(french_ok)puts("FRENCH DRIVER INSTALLED");}
  /* Physical AZERTY scan codes for 'echo azerty' followed by Enter. */
  if(frame>=1100&&frame<1172){static const uint8_t key[]={18,46,35,24,57,16,17,18,19,20,21,28};int phase=(frame-1100)%6;if(phase==0)pc_key(key[(frame-1100)/6],true);if(phase==3)pc_key(key[(frame-1100)/6],false);}
  pc_run_frame();
 }
 dump_screen();int w,h;pc_screen_signature();pc_screen_size(&w,&h);
 fprintf(stderr,"Frames=%u VGA=%dx%d CPU=%04x:%04x halted=%d\n",(unsigned)pc.frames,w,h,pc.cpu.seg[X86_CS].sel,pc.cpu.ip,pc_halted());
 return pc_halted()?6:!french_ok?8:0;
}
