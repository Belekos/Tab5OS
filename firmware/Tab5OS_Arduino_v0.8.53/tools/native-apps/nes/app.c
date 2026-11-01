#include "../common/runtime.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "core/nofrendo.h"
#include "core/nes/state.h"
static uint16_t* pixels;static uint16_t* palette;
static void video(uint8_t* buffer){for(int y=0;y<240;++y)for(int x=0;x<256;++x)pixels[y*256+x]=palette[buffer[y*272+x+8]];tab5_api->video_present(pixels,256*240);}
static void save_game(const char* sram,const char* state){nes_t* n=nes_getptr();if(n->cart->battery)rom_savesram(sram);int result=state_save(state);tab5_message(result?"Save failed.":"Game saved.",result?"Échec de la sauvegarde.":"Partie sauvegardée.");}
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){
 const struct tab5_app_api2* api=tab5_app_api_v2();if(!api||api->base.size<sizeof(*api))return 1;tab5_runtime_init(api);if(__builtin_setjmp(tab5_escape))return 7;
 if(argc<2||!argv[1][0]){tab5_message("Choose a .nes ROM.","Choisissez une ROM .nes.");return 2;}
 int fd=api->base.open_read(argv[1]);if(fd<0)return 2;size_t bytes=api->file_size(fd);uint8_t header[16]={0};
 int ok=bytes>=16&&bytes<=2u*1024u*1024u&&api->base.read(fd,header,16)==16&&!memcmp(header,"NES\032",4)&&header[4]&&(header[7]&12)!=8;
 size_t expected=16u+((header[6]&4)?512:0)+(size_t)header[4]*16384u+(size_t)header[5]*8192u;
 if(!ok||expected!=bytes){api->base.close(fd);tab5_message("Incomplete or unsupported iNES cartridge.","Cartouche iNES incomplète ou incompatible.");return 3;}
 tab5_runtime_capacity(bytes+1024u*1024u);uint8_t* data=malloc(bytes);api->base.seek(fd,0);ok=data&&tab5_load(fd,data,bytes);api->base.close(fd);if(!ok)return 4;
 uint32_t crc=tab5_crc32(0,data,bytes);nes_t* n=nes_init(SYS_DETECT,22050,true,NULL);if(!n||nes_insertcart(rom_loadmem(data,bytes))<0)return 5;
 pixels=malloc(256*240*2);palette=nofrendo_buildpalette(NES_PALETTE_NESCLASSIC,16);uint8_t* frame=calloc(272*240,1);if(!pixels||!palette||!frame)return 6;
 nes_setvidbuf(frame);n->blit_func=video;input_connect(0,NES_JOYPAD);input_connect(1,NES_JOYPAD);nes_reset(true);
 char sav[80],state[80];tab5_save_name(sav,"NES","sav",crc);tab5_save_name(state,"NES","state",crc);rom_loadsram(sav);
 nes_emulate(false);nes_emulate(false);state_load(state);
 if(api->video_begin(256,240))return 6;
 uint32_t fraction=0;uint32_t deadline=api->base.millis(),lastSave=deadline;int skip=0;
 while(!api->base.should_stop()){
  uint32_t k=api->buttons();if(k&256)break;if(k&512)save_game(sav,state);if(k&1024)nes_reset(true);
  unsigned pad=((k&16)>>4)|((k&32)>>4)|((k&64)>>4)|((k&128)>>4)|((k&4)<<2)|((k&8)<<2)|((k&2)<<5)|((k&1)<<7);input_update(0,pad);
  nes_setvidbuf(frame);nes_emulate(!skip);api->audio_pcm(n->apu->buffer,n->apu->samples_per_frame,22050);
  fraction+=1000;deadline+=fraction/n->refresh_rate;fraction%=n->refresh_rate;int32_t wait=(int32_t)(deadline-api->base.millis());skip=wait<-20;
  api->base.sleep_ms(wait>0?wait:0);if(wait<-200)deadline=api->base.millis();
  if(api->base.millis()-lastSave>30000){if(n->cart->battery)rom_savesram(sav);lastSave=api->base.millis();}
 }
 save_game(sav,state);nes_shutdown();return 0;
}
