/* Relative DOS mouse frontend. Touch is a touchpad: move, tap for primary,
 * hold for secondary, or tap then hold the second touch to drag. The serial
 * UART preserves even clicks shorter than a guest frame. No guest interrupt
 * vector or application memory is patched. */
#ifndef TAB5_DOS_APP_MOUSE_H
#define TAB5_DOS_APP_MOUSE_H
#include <stdint.h>

static struct {
 int x,y,w,h,guest_w,guest_h,active,moved,drag,last_x,last_y,start_x,start_y;
 int fraction_x,fraction_y,mouse_x,mouse_y,mouse_guest;
 uint32_t began,last_tap,pulse_until;
 unsigned physical,touch;
 int tap_valid;
} dos_pointer;
static void dos_pointer_send(int dx,int dy){pc_mouse(dx,dy,(uint8_t)((dos_pointer.physical|dos_pointer.touch)&3u));}
static void dos_pointer_cancel(void){
 dos_pointer.active=dos_pointer.moved=dos_pointer.drag=dos_pointer.tap_valid=0;
 dos_pointer.physical=dos_pointer.touch=dos_pointer.pulse_until=0;
 dos_pointer.fraction_x=dos_pointer.fraction_y=0;dos_pointer_send(0,0);
}
static void dos_pointer_viewport(int x,int y,int w,int h,int guest_w,int guest_h){
 if(dos_pointer.w!=w||dos_pointer.h!=h||dos_pointer.x!=x||dos_pointer.y!=y)dos_pointer_cancel();
 dos_pointer.x=x;dos_pointer.y=y;dos_pointer.w=w;dos_pointer.h=h;
 dos_pointer.guest_w=guest_w;dos_pointer.guest_h=guest_h;
}
static int dos_pointer_game_area(int x,int y){
 /* UI buttons may sit alongside the picture; only the displayed guest is a
  * touchpad. An active drag still continues beyond this initial hit region. */
 return dos_pointer.w>0&&x>=dos_pointer.x&&x<dos_pointer.x+dos_pointer.w&&
        y>=dos_pointer.y&&y<dos_pointer.y+dos_pointer.h;
}
static void dos_pointer_tick(uint32_t now){
 if(dos_pointer.pulse_until&&(int32_t)(now-dos_pointer.pulse_until)>=0){dos_pointer.pulse_until=0;dos_pointer.touch=0;dos_pointer_send(0,0);}
 if(dos_pointer.active&&!dos_pointer.moved&&!dos_pointer.drag&&now-dos_pointer.began>=500){
  dos_pointer.drag=1;dos_pointer.touch=2;dos_pointer_send(0,0);
 }
}
static int dos_pointer_touch_down(int x,int y,uint32_t now){
 if(!dos_pointer_game_area(x,y))return 0;
 int delta_x=x-dos_pointer.start_x,delta_y=y-dos_pointer.start_y;
 int repeat=dos_pointer.tap_valid&&now-dos_pointer.last_tap<350&&delta_x*delta_x+delta_y*delta_y<576;
 dos_pointer.active=1;dos_pointer.moved=0;dos_pointer.drag=repeat;
 dos_pointer.start_x=dos_pointer.last_x=x;dos_pointer.start_y=dos_pointer.last_y=y;
 dos_pointer.began=now;dos_pointer.fraction_x=dos_pointer.fraction_y=0;
 dos_pointer.tap_valid=0;dos_pointer.pulse_until=0;dos_pointer.touch=repeat?1:0;
 dos_pointer_send(0,0);return 1;
}
static void dos_pointer_touch_move(int x,int y){
 if(!dos_pointer.active||!dos_pointer.w||!dos_pointer.h)return;
 int tx=x-dos_pointer.start_x,ty=y-dos_pointer.start_y;
 if(tx*tx+ty*ty>=16)dos_pointer.moved=1;
 /* Preserve subpixel motion while the DOS display is scaled. The driver's
  * mickey/pixel ratio and acceleration remain normal INT33 services. */
 int scaled_x=(x-dos_pointer.last_x)*dos_pointer.guest_w+dos_pointer.fraction_x;
 int scaled_y=(y-dos_pointer.last_y)*dos_pointer.guest_h+dos_pointer.fraction_y;
 dos_pointer.fraction_x=scaled_x%dos_pointer.w;dos_pointer.fraction_y=scaled_y%dos_pointer.h;
 dos_pointer.last_x=x;dos_pointer.last_y=y;
 dos_pointer_send(scaled_x/dos_pointer.w,scaled_y/dos_pointer.h);
}
static void dos_pointer_touch_up(uint32_t now){
 if(!dos_pointer.active)return;
 const int tap=!dos_pointer.moved&&!dos_pointer.drag&&now-dos_pointer.began<500;
 dos_pointer.active=0;dos_pointer.touch=tap?1:0;
 dos_pointer.pulse_until=tap?now+70:0;dos_pointer.last_tap=now;dos_pointer.tap_valid=tap;
 dos_pointer_send(0,0);
}
static void dos_pointer_mouse_position(int x,int y,unsigned buttons){
 dos_pointer.mouse_x=x;dos_pointer.mouse_y=y;
 dos_pointer.mouse_guest=dos_pointer_game_area(x,y);
 dos_pointer.physical=dos_pointer.mouse_guest?(buttons&3u):0;dos_pointer_send(0,0);
}
static void dos_pointer_mouse_relative(int dx,int dy){
 if(dos_pointer.mouse_guest)dos_pointer_send(dx,dy);
}
#endif
