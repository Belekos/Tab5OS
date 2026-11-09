#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include "pc.h"
static char screen[80*25+1];
static unsigned presents;
uint32_t get_uticks(void){struct timespec t;timespec_get(&t,TIME_UTC);return (uint32_t)((uint64_t)t.tv_sec*1000000+t.tv_nsec/1000);}
void *bigmalloc(size_t size){return calloc(1,size);}
int load_rom(void *mem,const char *path,uword addr,int backward){
 FILE*f=fopen(path,"rb");if(!f){perror(path);exit(2);}fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 if(n<1||n>131072){fclose(f);exit(3);}size_t r=fread((uint8_t*)mem+addr-(backward?n:0),1,n,f);fclose(f);if(r!=(size_t)n)exit(4);return (int)n;
}
static void draw(void *o,int x,int y,int w,int h){(void)o;(void)x;(void)y;(void)w;(void)h;presents++;}
static void cursor(void*o,int y,int x){(void)o;(void)y;(void)x;}
static void put_char(void*o,int y,int x,int ch,int attr){(void)o;(void)attr;if(x>=0&&x<80&&y>=0&&y<25)screen[y*80+x]=(char)ch;}
static void refresh(void*o,const uint8_t *ram,int w,int h){(void)o;for(int y=0;y<25&&y<h;y++)for(int x=0;x<80&&x<w;x++)screen[y*80+x]=(char)ram[(y*w+x)*4];}
int main(int argc,char**argv){
 SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 if(argc!=4){fprintf(stderr,"boot_host bios vgabios floppy\n");return 2;}
 PCConfig c={0};c.mem_size=8*1024*1024;c.vga_mem_size=1024*1024;c.width=720;c.height=480;c.cpu_gen=4;c.bios=argv[1];c.vga_bios=argv[2];c.fdd[0]=argv[3];
 uint8_t *fb=calloc(1,720*480*2);PC*p=pc_new(draw,0,fb,&c);VGATextOps ops={cursor,put_char,refresh};vga_set_text_ops(p->vga,&ops,0);load_bios_and_reset(p);
 uint32_t start=get_uticks(),last=0,report=start;memset(screen,' ',sizeof(screen)-1);
 while((uint32_t)(get_uticks()-start)<15000000){pc_step(p);uint32_t n=get_uticks();if(n-last>15000){pc_vga_step(p);last=n;}
 if(n-report>1000000){CPUI386_State st={0};cpui386_get_state(p->cpu,&st);fprintf(stderr,"Progress %u: %x:%x cycles=%ld exc=%d ax=%x bx=%x dx=%x sp=%x\n",(n-start)/1000,st.seg[1],st.ip,cpui386_get_cycle(p->cpu),cpui386_get_excno(p->cpu),st.gpr[0],st.gpr[3],st.gpr[2],st.gpr[4]);fflush(stderr);report=n;}
 if(strstr(screen,"TAB5 PROTOTYPE DONE")){int ok=strstr(screen,"TAB5 386 DOS OK")&&strstr(screen,"TAB5 486 DOS OK")&&strstr(screen,"TAB5 PM32 RETURN OK")&&!strstr(screen,"DOS FAIL");printf("%s FreeDOS COM386 + MZ486 + PM32 return; time_ms=%u frames=%u cycles=%ld\n",ok?"PASS":"FAIL",(n-start)/1000,presents,cpui386_get_cycle(p->cpu));for(int y=0;y<25;y++)printf("%.80s\n",screen+y*80);return ok?0:1;}}
 for(int y=0;y<25;y++)printf("%.80s\n",screen+y*80);fprintf(stderr,"FAIL no FreeDOS marker; frames=%u\n",presents);return 1;
}
