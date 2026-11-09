#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../presentation.h"
static uint16_t expected[800*480],actual[800*480],line[1024];
static int source_width;
static void source(int y,uint16_t *out){for(int x=0;x<source_width;x++)out[x]=(uint16_t)(x*37+y*71+(x^y));}
static void reference(uint16_t *dst,int w,int h,int dw,int dh){for(int y=0;y<dh;y++){source(y*h/dh,line);for(int x=0;x<dw;x++)dst[y*800+x]=line[x*w/dw];}}
int main(void){
 const int cases[][4]={{720,400,720,400},{320,200,320,200},{640,480,533,400},{720,400,453,252},{1024,768,533,400},{640,350,460,252},{80,25,800,400},{1,1,1,1},{641,401,639,399}};
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  int w=cases[i][0],h=cases[i][1],dw=cases[i][2],dh=cases[i][3];source_width=w;
  for(unsigned p=0;p<800*480;p++)expected[p]=actual[p]=0xbeef;
  reference(expected,w,h,dw,dh);dos_present_screen(actual,800,w,h,dw,dh,line,source);
  assert(!memcmp(expected,actual,sizeof(actual)));
 }
 source_width=720;clock_t a=clock();for(int n=0;n<200;n++)reference(expected,720,400,720,400);clock_t b=clock();
 for(int n=0;n<200;n++)dos_present_screen(actual,800,720,400,720,400,line,source);clock_t c=clock();
 assert(!memcmp(expected,actual,sizeof(actual)));
 printf("{\"passed\":true,\"cases\":%u,\"oldHostMs\":%.3f,\"newHostMs\":%.3f,\"removedPixelDivisionsPer720x400Frame\":288000}\n",(unsigned)(sizeof(cases)/sizeof(cases[0])),1000.0*(b-a)/CLOCKS_PER_SEC,1000.0*(c-b)/CLOCKS_PER_SEC);
 return 0;
}
