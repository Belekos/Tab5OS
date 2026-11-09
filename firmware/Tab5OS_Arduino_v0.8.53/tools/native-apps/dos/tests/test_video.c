/* Compare asynchronous snapshots with same-thread live VRAM using the real
 * BIOS/VGA renderer. No copyrighted game files or physical-device claims. */
#include "pc_internal.h"
#include "video.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
extern uint8_t *vga_planes(void);
static uint32_t checksum(const uint16_t *pixels,int n,uint32_t hash){for(int i=0;i<n;i++)hash=(hash^pixels[i])*16777619u;return hash;}
int main(int argc,char **argv){
 pc_config_t cfg={0};cfg.board=PC_AT;cfg.cpu=X86_80286;cfg.cpu_hz=6000000;cfg.video=PC_VGA;cfg.conv_kb=640;cfg.ext_kb=1024;cfg.synchronous_video=argc>1&&argv[1][0]=='1';
 assert(!posix_memalign((void**)&cfg.ram,4096,PC_RAM_BYTES(cfg.ext_kb)));cfg.page_table=calloc(X86_PAGES,sizeof(uintptr_t));assert(pc_init(&cfg));
 const unsigned modes[]={3,4,0x0d,0x10,0x12,0x13};uint16_t line[1024];uint32_t results[12];int count=0;
 for(unsigned mode=0;mode<sizeof(modes)/sizeof(modes[0]);mode++){
  bios_video_mode((uint8_t)modes[mode]);uint8_t *vram=vga_planes();for(unsigned i=0;i<262144;i++)vram[i]=(uint8_t)(i*37u+(i>>8)+mode*13u);
  for(unsigned blink=0;blink<2;blink++){
   pc.frames=32+blink*8;pc_video_capture_next(true);video_frame_end();video_frame_end();
   uint32_t sig=pc_screen_signature();int w,h;pc_screen_size(&w,&h);assert(w>0&&w<=1024&&h>0&&h<=768);
   uint32_t hash=2166136261u;for(int y=0;y<h;y++){pc_render_line_rgb565(y,line);hash=checksum(line,w,hash);}results[count++]=hash;
   if(cfg.synchronous_video){pc_video_capture_next(false);vram[0]^=0x55;video_frame_end();assert(pc_screen_signature()==sig);vram[0]^=0x55;pc_video_capture_next(true);video_frame_end();assert(pc_screen_signature()==sig);}
  }
 }
 clock_t start=clock();for(unsigned i=0;i<3000;i++)video_frame_end();clock_t finish=clock();
 printf("{\"synchronous\":%s,\"frameChecksums\":[",cfg.synchronous_video?"true":"false");for(int i=0;i<count;i++)printf("%s%u",i?",":"",results[i]);
 printf("],\"snapshotHostMs\":%.3f,\"snapshotIterations\":3000,\"vramCopiesPerCapturedFrame\":%u}\n",1000.0*(finish-start)/CLOCKS_PER_SEC,cfg.synchronous_video?0:1);return 0;
}
