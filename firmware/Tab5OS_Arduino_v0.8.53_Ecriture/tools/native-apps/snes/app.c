#include "../common/runtime.h"
#include "core/snes9x.h"
#include "core/soundux.h"
#include "core/memmap.h"
#include "core/apu.h"
#include "core/display.h"
#include "core/cpuexec.h"
#include "core/gfx.h"
#include "core/ppu.h"
#include "core/save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t pad;static uint16_t* pixels;static char sav[80],state[80];
bool overclock_cycles=false;int one_c=6,slow_one_c=8,two_c=12;
bool S9xInitDisplay(void){GFX.Pitch=512*2;GFX.ZPitch=512;GFX.Screen=calloc(512*240*4,1);GFX.SubScreen=GFX.Screen+512*240*2;GFX.ZBuffer=calloc(512*240*2,1);GFX.SubZBuffer=GFX.ZBuffer+512*240;return GFX.Screen&&GFX.ZBuffer;}
void S9xDeinitDisplay(void){}void S9xToggleSoundChannel(int32_t c){(void)c;}
bool S9xReadMousePosition(int32_t w,int32_t* x,int32_t* y,uint32_t* b){(void)w;(void)x;(void)y;(void)b;return false;}
bool S9xReadSuperScopePosition(int32_t* x,int32_t* y,uint32_t* b){(void)x;(void)y;(void)b;return false;}
bool JustifierOffscreen(void){return true;}void JustifierButtons(uint32_t* j){(void)j;}
uint32_t S9xReadJoypad(int32_t p){if(p)return 0;uint32_t r=0;const uint32_t m[]={SNES_RIGHT_MASK,SNES_LEFT_MASK,SNES_UP_MASK,SNES_DOWN_MASK,SNES_A_MASK,SNES_B_MASK,SNES_SELECT_MASK,SNES_START_MASK};for(unsigned i=0;i<8;++i)if(pad&(1u<<i))r|=m[i];if(pad&(1u<<11))r|=SNES_X_MASK;if(pad&(1u<<12))r|=SNES_Y_MASK;if(pad&(1u<<13))r|=SNES_TL_MASK;if(pad&(1u<<14))r|=SNES_TR_MASK;return r;}
static size_t sram_bytes(void){return Memory.SRAMSize?(Memory.SRAMSize>7?0x20000:1u<<(Memory.SRAMSize+10)):0;}
static void save_game(void){size_t n=sram_bytes();if(n>0x20000)n=0x20000;if(n)tab5_api->save(sav,Memory.SRAM,n);if(!S9xSaveState(state))tab5_message("Save failed.","Échec de la sauvegarde.");}
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){
 const struct tab5_app_api4* api=tab5_app_api_v4();if(!api)return 1;tab5_runtime_init(&api->base.base);if(__builtin_setjmp(tab5_escape))return 8;
 if(argc<2||!argv[1][0]){tab5_message("Choose a .sfc or .smc ROM.","Choisissez une ROM .sfc ou .smc.");return 2;}int f=tab5_api->base.open_read(argv[1]);if(f<0)return 2;size_t bytes=tab5_api->file_size(f);if(bytes<32768||bytes>MAX_ROM_SIZE+512){tab5_api->base.close(f);return 3;}
 tab5_runtime_capacity(10u*1024u*1024u);Settings.CyclesPercentage=100;Settings.H_Max=SNES_CYCLES_PER_SCANLINE;Settings.FrameTimePAL=20000;Settings.FrameTimeNTSC=16667;Settings.ControllerOption=SNES_JOYPAD;Settings.HBlankStart=(256*Settings.H_Max)/SNES_HCOUNTER_MAX;Settings.SoundPlaybackRate=32000;Settings.DisableSoundEcho=false;Settings.InterpolatedSound=true;
 if(!S9xInitDisplay()||!S9xInitMemory()||!S9xInitAPU()||!S9xInitSound(0,0)||!S9xInitGFX())return 4;
 size_t offset=(bytes&2047)==512?512:0;tab5_api->base.seek(f,offset);int ok=tab5_load(f,Memory.ROM,bytes-offset);tab5_api->base.close(f);if(!ok)return 5;Memory.ROM_Size=bytes-offset;uint32_t crc=tab5_crc32(0,Memory.ROM,bytes-offset);if(!LoadROM(NULL))return 6;
 tab5_save_name(sav,"SNES","sav",crc);tab5_save_name(state,"SNES","state",crc);f=tab5_api->base.open_read(sav);if(f>0){size_t n=tab5_api->file_size(f);if(n==sram_bytes()&&n<=0x20000)tab5_load(f,Memory.SRAM,n);tab5_api->base.close(f);}S9xLoadState(state);S9xSetPlaybackRate(32000);
 pixels=calloc(256*240,2);int16_t* samples=malloc(2048*4);if(tab5_api->video_begin(256,240))return 7;uint32_t deadline=tab5_api->base.millis(),lastSave=deadline,fraction=0;int skip=0;unsigned fps=Settings.PAL?50:60;
 while(!tab5_api->base.should_stop()){
  pad=tab5_api->buttons();if(pad&256)break;if(pad&512)save_game();if(pad&1024)S9xReset();IPPU.RenderThisFrame=!skip;S9xMainLoop();
  if(!skip){unsigned h=IPPU.RenderedScreenHeight;if(h>240)h=240;unsigned w=IPPU.RenderedScreenWidth;if(w>512)w=512;memset(pixels,0,256*240*2);for(unsigned y=0;y<h;++y)for(unsigned x=0;x<256;++x)pixels[y*256+x]=((uint16_t*)(GFX.Screen+y*GFX.Pitch))[x*w/256];tab5_api->video_present(pixels,256*240);}
  size_t n=32000/fps;S9xMixSamples(samples,n*2);tab5_api->audio_pcm(samples,n,32000);fraction+=1000;deadline+=fraction/fps;fraction%=fps;int32_t wait=deadline-tab5_api->base.millis();skip=wait<-20;tab5_api->base.sleep_ms(wait>0?wait:0);if(wait<-200)deadline=tab5_api->base.millis();if(tab5_api->base.millis()-lastSave>30000){size_t n=sram_bytes();if(n&&n<=0x20000)tab5_api->save(sav,Memory.SRAM,n);lastSave=tab5_api->base.millis();}
 }
 save_game();S9xDeinitGFX();S9xDeinitAPU();S9xDeinitMemory();return 0;
}
