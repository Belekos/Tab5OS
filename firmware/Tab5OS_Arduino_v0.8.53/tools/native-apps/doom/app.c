#include "../common/runtime.h"
#include "../common/game_runtime.h"
#include "psram_app.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern void tab5_doom_free_caches(void);
extern int SCREENWIDTH,SCREENHEIGHT,myargc;extern char** myargv;
extern void Z_Init(void),D_DoomMain(void);
const app_services_t* _papp_svc;
volatile int papp_exit_requested;
uint32_t tab5_game_buttons;
static int result_code=9;static uint32_t audio_rate=22050;
void app_return_to_launcher(void){result_code=0;papp_exit_requested=1;__builtin_longjmp(tab5_escape,1);}
void papp_panic(const char* msg){puts(msg);result_code=9;__builtin_longjmp(tab5_escape,1);}
static void delay(int ms){tab5_api->base.sleep_ms(ms>0?ms:0);if(tab5_api->base.should_stop())app_return_to_launcher();}
static int64_t micros(void){return (int64_t)tab5_api->base.millis()*1000;}
static void* alloc_caps(size_t n,uint32_t caps){(void)caps;return malloc(n);}
static void gamepad(papp_gamepad_state_t* p){memset(p,0,sizeof(*p));uint32_t k=tab5_api->buttons();tab5_game_buttons=k;if(tab5_api->base.should_stop()||(k&256))app_return_to_launcher();
 const int dest[]={PAPP_INPUT_RIGHT,PAPP_INPUT_LEFT,PAPP_INPUT_UP,PAPP_INPUT_DOWN,PAPP_INPUT_A,PAPP_INPUT_B,PAPP_INPUT_SELECT,PAPP_INPUT_START};for(int i=0;i<8;++i)p->values[dest[i]]=(k>>i)&1;
 p->values[PAPP_INPUT_X]=(k>>11)&1;p->values[PAPP_INPUT_Y]=(k>>12)&1;p->values[PAPP_INPUT_L]=(k>>13)&1;p->values[PAPP_INPUT_R]=(k>>14)&1;p->values[PAPP_INPUT_MENU]=(k>>10)&1;}
static int vlog(const char* fmt,va_list ap){char out[512];int n=vsnprintf(out,sizeof(out),fmt,ap);tab5_api->base.log(out,strlen(out));return n;}
static void video(const uint16_t* p,uint16_t w,uint16_t h,float scale,bool swap){(void)scale;(void)swap;if(w!=320||h!=200)abort();tab5_api->video_present(p,w*h);}
static void audio_init(int rate){audio_rate=rate;}
static void audio(short* samples,int count){if(count>0&&count<=2048)tab5_api->audio_pcm(samples,count,audio_rate);}
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){
 const struct tab5_app_api4* api=tab5_app_api_v4();if(!api)return 1;tab5_runtime_init(&api->base.base);if(__builtin_setjmp(tab5_escape))return result_code;
 if(argc<2||!argv||!argv[1]||!argv[1][0]){tab5_message("Choose your Doom .wad file.","Choisissez votre fichier Doom .wad.");return 2;}
 printf("Doom: opening %s\n",argv[1]);
 tab5_runtime_capacity(10*1024*1024);tab5_runtime_game(argv[1],"Doom");
 static app_services_t services;services.abi_version=1;services.mem_alloc=malloc;services.mem_calloc=calloc;services.mem_realloc=realloc;services.mem_free=free;services.mem_caps_alloc=alloc_caps;services.log_printf=printf;services.log_vprintf=vlog;services.input_gamepad_read=gamepad;services.get_time_us=micros;services.delay_ms=delay;services.audio_init=audio_init;services.audio_submit=audio;services.display_write_frame_custom=video;_papp_svc=&services;
 SCREENWIDTH=320;SCREENHEIGHT=200;if(tab5_api->video_begin(320,200)){puts("Doom: unable to create the game display; close other applications and try again.");return 3;}
 char* args[]={"doom","-save",(char*)tab5_game_saves(),"-iwad",argv[1],"-config","doom.cfg",NULL};myargc=7;myargv=args;Z_Init();tab5_runtime_reclaimer(tab5_doom_free_caches);D_DoomMain();return 0;
}
