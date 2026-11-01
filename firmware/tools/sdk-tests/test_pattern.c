#include "Tab5AppApi.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
static int retained,deny,video_fail,frames,stop_now;
static void* allocate(size_t n){if(deny)return NULL;++retained;return malloc(n);}
static void release(void *p){if(p){--retained;free(p);}}
static int stop(void){return stop_now;}
static int sleep_for(uint32_t n){assert(n>0&&n<=100);return frames>=3;}
static int video_begin(uint16_t w,uint16_t h){assert(w==128&&h==96);return video_fail;}
static int video_present(const uint16_t *p,size_t n){assert(p&&n==128*96);++frames;return 0;}
static uint32_t buttons(void){return 0;}
static struct tab5_app_api2 api;
const struct tab5_app_api2* tab5_app_api_v2(void){return &api;}
int tab5_app_main(int,char**);
int main(void){
 api.base.version=TAB5_APP_ABI_V2;api.base.size=sizeof(api);api.base.allocate=allocate;api.base.release=release;api.base.should_stop=stop;api.base.sleep_ms=sleep_for;api.video_begin=video_begin;api.video_present=video_present;api.buttons=buttons;
 for(int i=0;i<2000;++i){frames=0;assert(tab5_app_main(0,NULL)==0);assert(frames==3&&retained==0);}
 deny=1;assert(tab5_app_main(0,NULL)==2&&retained==0);deny=0;
 video_fail=1;assert(tab5_app_main(0,NULL)==3&&retained==0);video_fail=0;
 stop_now=1;frames=0;assert(tab5_app_main(0,NULL)==0&&frames==0&&retained==0);
 api.base.version=99;assert(tab5_app_main(0,NULL)==1&&retained==0);
 puts("PASS: graphical SDK example, 2000 lifecycles, allocation failure, video failure, cancellation and ABI rejection");
}
