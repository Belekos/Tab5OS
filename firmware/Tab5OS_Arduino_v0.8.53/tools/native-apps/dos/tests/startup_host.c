/* Run the production startup sequencer against the actual FreeDOS shell. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_internal.h"
uint8_t *vga_planes(void);
bool dos_keyboard_typing(void);
extern int dos_french_keyboard;
#include "../startup.h"
static char screen[2026];
static void read_screen(void) {
    for (unsigned y=0;y<25;y++) {
        for (unsigned x=0;x<80;x++) {
            unsigned c=vga_planes()[(y*80+x)*8];
            screen[y*81+x]=(char)(c>=32&&c<127?c:' ');
        }
        screen[y*81+80]='\n';
    }
    screen[2025]=0;
}
static uint32_t vectors(void) {
    const unsigned table[]={9,0x15,0x16,0x2f}; uint32_t h=2166136261u;
    for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)h=(h^pc.ram[table[i]*4+j])*16777619u;
    return h;
}
int main(int argc,char **argv) {
    if(argc!=6)return 2;
    int french=atoi(argv[4]),driver=atoi(argv[5]);
    pc_config_t cfg={0};cfg.board=PC_AT;cfg.cpu=X86_80286;cfg.cpu_hz=6000000;
    cfg.video=PC_VGA;cfg.conv_kb=640;cfg.ext_kb=1024;
    if(posix_memalign((void**)&cfg.ram,4096,PC_RAM_BYTES(cfg.ext_kb)))return 3;
    cfg.page_table=calloc(X86_PAGES,sizeof(uintptr_t));
    if(!cfg.page_table||!pc_init(&cfg)||!pc_floppy_insert(0,argv[1])||
       !pc_hd_insert(0,argv[3])||(driver&&!pc_floppy_insert(1,argv[2])))return 4;
    pc_reset();struct dos_startup startup={0};
    int pending=0,launched=0,query=0,mouse=0;uint32_t before=0,since=0;
    for(unsigned frame=0;frame<6000;frame++) {
        pc_run_frame();read_screen();
        if(strstr(screen,"Installed at COM1"))mouse=1;
        if(!startup.step&&strstr(screen,"TAB5OS DOS READY"))dos_startup_begin(&startup);
        if(pending&&!dos_keyboard_typing()&&vectors()!=before&&pc.frames-since>5){
            dos_french_keyboard=1;pending=0;
        }
        int result=dos_startup_poll(&startup,driver,french,pending,"HELLO.EXE");
        if(result==1){before=vectors();since=pc.frames;pending=1;pc_type("keyb fr,437,a:\\keyboard.sys\n");}
        if(result<0){fprintf(stderr,"Startup failed: %d\n%s",result,screen);return 5;}
        if(strstr(screen,"Bad command or filename")){fprintf(stderr,"Lost startup command:\n%s",screen);return 6;}
        if(strstr(screen,"TAB5 MZ EXE OK")&&strstr(screen,"TAB5 ADJACENT DATA OK"))launched=1;
        if(launched&&!query&&dos_shell_prompt()=='C'){pc_type("set blaster\n");query=1;}
        if(query&&dos_shell_prompt()=='C'&&(strstr(screen,"BLASTER=a220 i7 d1 t3")||strstr(screen,"blaster=a220 i7 d1 t3"))) {
            if((driver&&!mouse)||(french&&!dos_french_keyboard))break;
            printf("STARTUP PASS french=%d driver=%d frames=%u: full BLASTER value, serial driver, C: launch and adjacent file\n",french,driver,pc.frames);return 0;
        }
    }
    fprintf(stderr,"Startup timeout step=%u pending=%d launched=%d query=%d mouse=%d:\n%s",startup.step,pending,launched,query,mouse,screen);
    return 7;
}
