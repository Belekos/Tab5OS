/* Execute the actual app.c render regions with a deterministic guest picture.
 * The PC, disk and audio callbacks are fixtures; pixels and hit boxes come
 * from the production renderer, not a second UI implementation. */
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "../presentation.h"
#define W 800
#define H 480
static uint16_t guarded[W*H+4],scratch[1024];
static uint16_t *pixels=guarded+2,*line=scratch;
static int keyboard_visible,info_visible,ui_dirty=1,dos_french_keyboard,shift,control,alt,quit;
static uint16_t held_touch;
static char notice[96]="DOS changes saved. Use the native Quit button to save and exit.";
static char performance[40]="PC 56% | 6 fps";
static char stats_details[3][48]={"CPU 1220 ms | Audio 290 ms","Video 340 ms | Period 2000 ms","DOS heap free 4096K | block 2048K"};
static int guest_w=320,guest_h=200,text_mode,dirty_disk,muted,rendered_frames,hit_checks;
static int saves,cancels;
static uint32_t signature=1;
static struct dos_viewport pointer_view;
extern const uint8_t pc_font_bios8[1024];
static uint32_t pc_screen_signature(void){return signature;}
static void pc_screen_size(int *w,int *h){*w=guest_w;*h=guest_h;}
static const char *video_mode_name(void){return text_mode?"80 x 25 text":"320 x 200, 256 colours";}
static int dos_overlay_dirty(void){return dirty_disk;}
static int dos_audio_failed(void){return 0;}
static int dos_audio_volume(void){return muted?0:60;}
static int dos_overlay_save(void){saves++;return 0;}
static void dos_pointer_cancel(void){cancels++;}
static void change_layout(void){}
static void dos_audio_change(int value){(void)value;}
static void dos_audio_mute(void){muted=!muted;}
static void input_key(uint16_t scan,int down,unsigned source){(void)scan;(void)down;(void)source;}
static void message(const char *s){snprintf(notice,sizeof(notice),"%s",s);}
static void pc_type(const char *s){(void)s;}
static void dos_pointer_viewport(int x,int y,int w,int h,int gw,int gh){
 (void)gw;(void)gh;pointer_view=(struct dos_viewport){x,y,w,h};
}
static void pc_render_line_rgb565(int y,uint16_t *row){
 for(int x=0;x<guest_w;x++){
  int dx=(x-guest_w/2)*5,dy=(y-guest_h/2)*6;
  row[x]=dx*dx+dy*dy<90000?0xfca0:(uint16_t)(((x/16+y/16)&1)?0x194a:0x216c);
 }
}
static int present(const uint16_t *p,size_t count){
 assert(p==pixels&&count==W*H);rendered_frames++;
 assert(guarded[0]==0xbeef&&guarded[1]==0xbeef&&guarded[W*H+2]==0xbeef&&guarded[W*H+3]==0xbeef);return 0;
}
static const struct {int (*video_present)(const uint16_t*,size_t);} api={present};
#define tab5_api (&api)
#include "ui_primitives.inc"
#include "ui_actions.inc"
#include "ui_render.inc"
static int intersects(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh){
 return ax<bx+bw&&bx<ax+aw&&ay<by+bh&&by<ay+ah;
}
static void check_hits(void){
 assert(hit_count>0&&hit_count<100);
 for(int i=0;i<hit_count;i++){
  struct hit a=hits[i];assert(a.w>0&&a.h>0&&a.x>=0&&a.y>=0&&a.x+a.w<=W&&a.y+a.h<=H);hit_checks++;
  if(!info_visible)assert(!intersects(a.x,a.y,a.w,a.h,pointer_view.x,pointer_view.y,pointer_view.w,pointer_view.h));
  for(int j=i+1;j<hit_count;j++){struct hit b=hits[j];assert(!intersects(a.x,a.y,a.w,a.h,b.x,b.y,b.w,b.h));}
 }
 if(info_visible)assert(hit_count==1&&hits[0].action==ACT_INFO);
}
static void picture(const char *directory,const char *name){
 if(!directory)return;char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
 FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",W,H);
 for(int i=0;i<W*H;i++){uint16_t c=pixels[i];unsigned char rgb[3]={(unsigned char)(((c>>11)&31)*255/31),(unsigned char)(((c>>5)&63)*255/63),(unsigned char)((c&31)*255/31)};assert(fwrite(rgb,1,3,f)==3);}
 assert(!fclose(f));
}
int main(int argc,char **argv){
 guarded[0]=guarded[1]=guarded[W*H+2]=guarded[W*H+3]=0xbeef;
 const char *directory=argc>1?argv[1]:NULL;
 assert(render());check_hits();assert(pointer_view.w==624&&pointer_view.h==468);picture(directory,"dos-graphics");
 assert(!render());
 /* Multiple queued presses must not dispatch through the previous layout. */
 assert(pointer_control(700,282)&&info_visible&&hit_count==0);
 assert(pointer_control(700,110)&&!saves); // old Save, now behind modal
 assert(render());check_hits();
 assert(pointer_control(680,60)&&!info_visible&&hit_count==0);
 assert(!pointer_control(680,60)); // old Close cannot reopen a hidden modal
 assert(render());check_hits();
 assert(pointer_control(700,20)&&keyboard_visible&&hit_count==0);
 assert(!pointer_control(700,110)&&!saves); // no stale sidebar Save
 assert(render());check_hits();assert(cancels==3);release_touch();
 keyboard_visible=1;ui_dirty=1;assert(render());check_hits();assert(pointer_view.w*3==pointer_view.h*4);picture(directory,"dos-keyboard");
 info_visible=1;ui_dirty=1;assert(render());check_hits();picture(directory,"dos-info");
 info_visible=keyboard_visible=0;text_mode=1;guest_w=720;guest_h=400;signature++;assert(render());check_hits();assert(pointer_view.x==40&&pointer_view.y==0&&pointer_view.w==720&&pointer_view.h==400);picture(directory,"dos-text");
 keyboard_visible=1;ui_dirty=1;assert(render());check_hits();
 text_mode=0;guest_w=640;guest_h=480;keyboard_visible=0;signature++;assert(render());check_hits();
 muted=1;dirty_disk=1;ui_dirty=1;assert(render());check_hits();
 printf("{\"passed\":true,\"renderedFrames\":%d,\"hitBoxesChecked\":%d,\"noOverlaps\":true,\"guardedCanvas\":true,\"modalIsolatesControls\":true}\n",rendered_frames,hit_checks);
 return 0;
}
