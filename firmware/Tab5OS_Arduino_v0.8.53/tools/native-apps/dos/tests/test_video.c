/* Compare asynchronous snapshots with same-thread live VRAM using the real
 * BIOS/VGA renderer. No copyrighted game files or physical-device claims. */
#include "pc_internal.h"
#include "video.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
extern uint8_t *vga_planes(void);
static uint32_t checksum(const uint16_t *pixels,int n,uint32_t hash){for(int i=0;i<n;i++)hash=(hash^pixels[i])*16777619u;return hash;}
static uint16_t reference[1024*768],actual[1024*768];
static size_t image_pixels;
static uint32_t capture(unsigned frame,uint16_t *pixels,int settle){
 pc.frames=frame;pc_video_capture_next(true);video_frame_end();if(settle)video_frame_end();
 uint32_t sig=pc_screen_signature();int w,h;pc_screen_size(&w,&h);assert(w>0&&w<=1024&&h>0&&h<=768);image_pixels=(size_t)w*h;
 for(int y=0;y<h;y++)pc_render_line_rgb565(y,pixels+y*w);return sig;
}
static void attr(unsigned index,unsigned value){uint8_t ignored;video_in(0x3da,&ignored);video_out(0x3c0,(uint8_t)(index|0x20));video_out(0x3c0,(uint8_t)value);}
static void crtc(unsigned index,unsigned value){video_out(0x3d4,(uint8_t)index);video_out(0x3d5,(uint8_t)value);}
static void check_animation(void){
 const unsigned modes[]={4,0x0d,0x10,0x12,0x13};
 for(unsigned m=0;m<sizeof(modes)/sizeof(modes[0]);m++){
  bios_video_mode((int)modes[m]);uint8_t *vram=vga_planes();memset(vram,0,262144);
  uint32_t sig=capture(0,reference,1);size_t n=image_pixels;
  /* Hold frame phase constant: actual VRAM writes must drive every update. */
  for(unsigned step=0;step<12;step++){
   vram[step*16]=(uint8_t)(modes[m]==0x13?step+1:0x80);
   uint32_t next=capture(0,actual,1);
   if(!(next!=sig&&image_pixels==n&&memcmp(reference,actual,n*2)))fprintf(stderr,"animation mode=%u step=%u old=%u new=%u pixel_difference=%d\n",modes[m],step,sig,next,memcmp(reference,actual,n*2));
   assert(next!=sig&&image_pixels==n&&memcmp(reference,actual,n*2));
   memcpy(reference,actual,n*2);sig=next;
  }
 }
 /* Palette entry 255 occupies the final three bytes of vga_regs_t. It must
  * invalidate a static framebuffer even without a frame-phase change. */
 bios_video_mode(0x13);memset(vga_planes(),255,262144);
 video_out(0x3c8,255);video_out(0x3c9,63);video_out(0x3c9,0);video_out(0x3c9,0);
 uint32_t sig=capture(0,reference,1);size_t n=image_pixels;
 video_out(0x3c8,255);video_out(0x3c9,0);video_out(0x3c9,63);video_out(0x3c9,0);
 assert(capture(0,actual,1)!=sig&&memcmp(reference,actual,n*2));
 /* Display-start changes must also refresh the image without VRAM writes. */
 for(unsigned i=0;i<262144;i++)vga_planes()[i]=(uint8_t)((i/128)%16);
 crtc(0x0c,0);crtc(0x0d,0);sig=capture(0,reference,1);n=image_pixels;
 crtc(0x0d,32);
 assert(capture(0,actual,1)!=sig&&memcmp(reference,actual,n*2));
}
static void check_phases(void){
 const unsigned modes[]={4,0x0d,0x10,0x12,0x13};
 for(unsigned m=0;m<sizeof(modes)/sizeof(modes[0]);m++){
  bios_video_mode((int)modes[m]);uint8_t *vram=vga_planes();for(unsigned i=0;i<262144;i++)vram[i]=(uint8_t)(i*37u+(i>>8));
  uint32_t sig=capture(0,reference,1);size_t n=image_pixels;
  for(unsigned frame=8;frame<=32;frame+=8){assert(capture(frame,actual,1)==sig);assert(image_pixels==n&&!memcmp(reference,actual,n*2));}
 }
 /* Explicit text fixtures separate the eight-frame cursor from character blink. */
 bios_video_mode(3);uint8_t *vram=vga_planes();vram[0]='A';vram[1]=0x87;crtc(0x0a,0x20);attr(0x10,0x0c);
 uint32_t sig=capture(0,reference,1);size_t n=image_pixels;
 assert(capture(8,actual,1)==sig&&!memcmp(reference,actual,n*2));
 assert(capture(16,actual,1)!=sig&&memcmp(reference,actual,n*2));
 assert(capture(32,actual,1)==sig&&!memcmp(reference,actual,n*2));
 attr(0x10,0x04);crtc(0x0a,0);crtc(0x0b,15);crtc(0x0e,0);crtc(0x0f,0);vram[1]=0x07;
 sig=capture(0,reference,1);n=image_pixels;
 assert(capture(8,actual,1)!=sig&&memcmp(reference,actual,n*2));
 assert(capture(16,actual,1)==sig&&!memcmp(reference,actual,n*2));
 crtc(0x0a,0x20);sig=capture(0,reference,1);n=image_pixels;
 for(unsigned frame=8;frame<=32;frame+=8)assert(capture(frame,actual,1)==sig&&!memcmp(reference,actual,n*2));
 /* A palette write partway through graphics must still change the picture. */
 bios_video_mode(0x13);memset(vga_planes(),1,262144);sig=capture(0,reference,1);n=image_pixels;
 pc.frame_start=0;pc.cpu.clk=0;pc.cpu.cycles=video_frame_clocks()/3;
 int split,dot,lines,dots;video_beam(&split,&dot,&lines,&dots);assert(split>0&&split<400);
 video_out(0x3c8,1);video_out(0x3c9,63);video_out(0x3c9,0);video_out(0x3c9,0);
 assert(capture(0,actual,0)!=sig&&memcmp(reference,actual,n*2));
 int w,h;pc_screen_size(&w,&h);assert(!memcmp(reference,actual,(size_t)split*w*2));
 assert(memcmp(reference+(size_t)split*w,actual+(size_t)split*w,(size_t)(h-split)*w*2));
 pc.cpu.cycles=pc.frame_start=0;
 /* CGA can switch from graphics to blinking text within one frame. */
 pc.cfg.video=PC_CGA;video_reset();
 uint32_t mixed[2];for(unsigned pass=0;pass<2;pass++){
  bios_video_mode(4);for(unsigned i=0;i<16384;i+=2){pc.ram[0xb8000+i]='A';pc.ram[0xb8000+i+1]=0x87;}
  capture(pass*16,actual,1);pc.cpu.cycles=video_frame_clocks()/3;
  video_out(0x3d8,(uint8_t)((adapter.mode&~2u)|0x20));
  mixed[pass]=capture(pass*16,pass?actual:reference,0);
  pc.cpu.cycles=0;
 }
 assert(mixed[0]!=mixed[1]&&memcmp(reference,actual,image_pixels*2));
 pc.cfg.video=PC_VGA;video_reset();bios_video_mode(0x13);capture(0,actual,1);
}
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
 check_phases();
 check_animation();
 clock_t start=clock();for(unsigned i=0;i<3000;i++)video_frame_end();clock_t finish=clock();
 printf("{\"synchronous\":%s,\"frameChecksums\":[",cfg.synchronous_video?"true":"false");for(int i=0;i<count;i++)printf("%s%u",i?",":"",results[i]);
 printf("],\"staticGraphicsStable\":true,\"textBlinkAndCursor\":true,\"rasterPaletteAndMode\":true,\"animatedGraphicsRefresh\":true,\"palette255Refresh\":true,\"displayStartRefresh\":true,\"snapshotHostMs\":%.3f,\"snapshotIterations\":3000,\"vramCopiesPerCapturedFrame\":%u}\n",1000.0*(finish-start)/CLOCKS_PER_SEC,cfg.synchronous_video?0:1);return 0;
}
