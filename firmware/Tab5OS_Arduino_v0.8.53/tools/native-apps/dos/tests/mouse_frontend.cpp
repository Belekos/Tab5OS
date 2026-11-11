#include <cassert>
#include <cstdio>
#include <vector>
#include "../../../../Tab5AppInputQueue.h"
struct Motion {int x,y;unsigned buttons;};
static std::vector<Motion> motions;
static void pc_mouse(int x,int y,uint8_t buttons){motions.push_back({x,y,buttons});}
#include "../app_mouse.h"
static tab5_app_input_event event(unsigned type,int x,int y,unsigned buttons){
 tab5_app_input_event e{};e.size=sizeof(e);e.type=type;e.x=x;e.y=y;e.key=buttons;return e;
}
int main(){
 dos_pointer_viewport(80,36,640,400,320,200);
 dos_pointer_touch_down(200,100,1000);assert(motions.back().buttons==0);
 dos_pointer_touch_move(202,104);assert(motions.back().x==1&&motions.back().y==2);
 dos_pointer_touch_up(1100);assert(motions.back().buttons==0); // movement is not a click
 dos_pointer_touch_down(200,100,2000);dos_pointer_touch_up(2050);
 assert(motions.back().buttons==1);dos_pointer_tick(2121);assert(motions.back().buttons==0);
 dos_pointer_touch_down(201,101,2200);assert(motions.back().buttons==1); // second tap drags
 dos_pointer_touch_move(221,111);assert(motions.back().buttons==1&&motions.back().x==10);
 dos_pointer_touch_up(2250);assert(motions.back().buttons==0);
 dos_pointer_touch_down(200,100,3000);dos_pointer_tick(3500);assert(motions.back().buttons==2);
 dos_pointer_touch_up(3510);assert(motions.back().buttons==0);
 assert(!dos_pointer_touch_down(10,10,4000)); // toolbar
 dos_pointer_mouse_position(300,200,1);dos_pointer_mouse_relative(23,-17);
 assert(motions.back().x==23&&motions.back().y==-17&&motions.back().buttons==1);
 dos_pointer_mouse_position(300,200,2);assert(motions.back().buttons==2);
 dos_pointer_mouse_position(-1,-1,3);assert(motions.back().buttons==0);
 dos_pointer_mouse_position(300,200,1);dos_pointer_cancel();assert(motions.back().buttons==0);
 dos_pointer_touch_down(200,100,5000);dos_pointer_tick(5500);
 dos_pointer_viewport(80,36,400,250,320,200);assert(motions.back().buttons==0); // layout releases
 dos_pointer_viewport(0,0,624,468,320,200);
 assert(dos_pointer_touch_down(10,10,6000));dos_pointer_touch_up(6010);dos_pointer_tick(6090);
 assert(!dos_pointer_touch_down(640,100,6100)); // sidebar is never guest input
 assert(!dos_pointer_touch_down(600,470,6100)); // notice strip
 dos_pointer_mouse_position(640,100,3);assert(motions.back().buttons==0);
 dos_pointer_viewport(232,0,336,252,320,200);
 assert(!dos_pointer_touch_down(240,260,6200)); // keyboard toolbar
 Tab5AppInputQueue q;
 for(int i=0;i<200;++i)q.pushPointer(event(TAB5_INPUT_POINTER_POSITION,i,i,0),event(TAB5_INPUT_POINTER_RELATIVE,2,-3,0));
 tab5_app_input_event e{};assert(q.pop(e)&&e.type==TAB5_INPUT_POINTER_POSITION&&e.x==199);
 assert(q.pop(e)&&e.type==TAB5_INPUT_POINTER_RELATIVE&&e.x==400&&e.y==-600);assert(!q.pop(e));
 for(unsigned button:{0u,1u,0u,2u,0u})q.pushPointer(event(TAB5_INPUT_POINTER_POSITION,20,30,button),event(TAB5_INPUT_POINTER_RELATIVE,1,2,button));
 for(unsigned button:{0u,1u,0u,2u,0u}){assert(q.pop(e)&&e.type==TAB5_INPUT_POINTER_POSITION&&e.key==button);assert(q.pop(e)&&e.type==TAB5_INPUT_POINTER_RELATIVE&&e.key==button);}
 assert(!q.pop(e));
 for(int i=0;i<16;++i)q.pushPointer(event(TAB5_INPUT_POINTER_POSITION,i,i,i%2),event(TAB5_INPUT_POINTER_RELATIVE,1,1,i%2));
 q.pushPointer(event(TAB5_INPUT_POINTER_POSITION,1,2,0),event(TAB5_INPUT_POINTER_RELATIVE,3,4,0));
 assert(q.pop(e)&&e.type==TAB5_INPUT_CANCEL);assert(q.pop(e)&&e.type==TAB5_INPUT_POINTER_POSITION);assert(q.pop(e)&&e.type==TAB5_INPUT_POINTER_RELATIVE);assert(!q.pop(e));
 puts("{\"passed\":true,\"component\":\"DOS touchpad, physical mouse and ordered event queue\"}");
}
