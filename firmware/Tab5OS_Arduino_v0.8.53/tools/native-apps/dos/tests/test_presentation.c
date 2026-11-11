#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../presentation.h"
static uint16_t expected[800*480],actual[800*480],line[1024];
static int source_width,source_calls;
static void source(int y,uint16_t *out){source_calls++;for(int x=0;x<source_width;x++)out[x]=(uint16_t)(x*37+y*71+(x^y));}
static void reference(uint16_t *dst,int w,int h,int dw,int dh){for(int y=0;y<dh;y++){source(y*h/dh,line);for(int x=0;x<dw;x++)dst[y*800+x]=line[x*w/dw];}}
int main(void){
 const int cases[][4]={{720,400,720,400},{320,200,320,200},{640,480,533,400},{720,400,453,252},{1024,768,533,400},{640,350,460,252},{80,25,800,400},{1,1,1,1},{641,401,639,399},{320,200,624,468},{320,200,336,252},{640,480,624,468}};
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  int w=cases[i][0],h=cases[i][1],dw=cases[i][2],dh=cases[i][3];source_width=w;
  for(unsigned p=0;p<800*480;p++)expected[p]=actual[p]=0xbeef;
  reference(expected,w,h,dw,dh);dos_present_screen(actual,800,w,h,dw,dh,line,source);
  assert(!memcmp(expected,actual,sizeof(actual)));
 }
 /* A 320x200 frame must decode only 200 guest rows despite enlargement. */
 source_width=320;source_calls=0;dos_present_screen(actual,800,320,200,624,468,line,source);assert(source_calls==200);
 const int modes[][2]={{320,200},{320,240},{640,350},{640,480},{720,400},{1024,768}};
 for(unsigned i=0;i<sizeof(modes)/sizeof(modes[0]);i++)for(int keyboard=0;keyboard<2;keyboard++){
  struct dos_viewport v=dos_display_viewport(modes[i][0],modes[i][1],0,keyboard);
  assert(v.w*3==v.h*4&&v.x>=0&&v.y>=0&&v.x+v.w<=800&&v.y+v.h<=480);
  assert(v.w==(keyboard?336:624)&&v.h==(keyboard?252:468));
 }
 struct dos_viewport text_view=dos_display_viewport(720,400,1,0);
 assert(text_view.x==40&&text_view.y==0&&text_view.w==720&&text_view.h==400);
 assert(dos_display_viewport(0,0,0,0).w==0&&dos_display_viewport(1025,768,0,0).w==0);
 for(unsigned p=0;p<800*480;p++)expected[p]=actual[p]=0xbeef;
 source_width=720;clock_t a=clock();for(int n=0;n<200;n++)reference(expected,720,400,720,400);clock_t b=clock();
 for(int n=0;n<200;n++)dos_present_screen(actual,800,720,400,720,400,line,source);clock_t c=clock();
 assert(!memcmp(expected,actual,sizeof(actual)));
 printf("{\"passed\":true,\"cases\":%u,\"crtAspect\":true,\"nativeTextGlyphs\":true,\"enlarged320x200DecodedRows\":200,\"oldHostMs\":%.3f,\"newHostMs\":%.3f,\"removedPixelDivisionsPer720x400Frame\":288000}\n",(unsigned)(sizeof(cases)/sizeof(cases[0])),1000.0*(b-a)/CLOCKS_PER_SEC,1000.0*(c-b)/CLOCKS_PER_SEC);
 return 0;
}
