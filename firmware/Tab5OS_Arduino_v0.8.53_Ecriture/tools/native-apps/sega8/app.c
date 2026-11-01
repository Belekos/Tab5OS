#include "../common/runtime.h"
#include "core/shared.h"
static char savepath[80],statepath[80];
void system_manage_sram(uint8* sram,int slot,int mode){(void)slot;if(mode==SRAM_SAVE){if(sms.save)tab5_api->save(savepath,sram,0x8000);}else{int f=tab5_api->base.open_read(savepath);if(f>0){if(tab5_api->file_size(f)==0x8000)tab5_load(f,sram,0x8000);tab5_api->base.close(f);}}}
static void save_game(void){system_manage_sram(cart.sram,0,SRAM_SAVE);FILE* f=fopen(statepath,"wb");int result=-1;if(f){system_save_state(f);result=fclose(f);}tab5_message(result?"Save failed.":"Game saved.",result?"Échec de la sauvegarde.":"Partie sauvegardée.");}
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){
 const struct tab5_app_api2* api=tab5_app_api_v2();if(!api||api->base.size<sizeof(*api))return 1;tab5_runtime_init(api);if(__builtin_setjmp(tab5_escape))return 8;
 if(argc<2||!argv[1][0]){tab5_message("Choose a .gg or .sms ROM.","Choisissez une ROM .gg ou .sms.");return 2;}
 int f=api->base.open_read(argv[1]);if(f<0)return 2;size_t size=api->file_size(f);if(size<8192||size>1024u*1024u||size%8192){api->base.close(f);return 3;}
 size_t alloc=(size+16383)&~16383u;tab5_runtime_capacity(alloc+1536u*1024u);uint8_t* rom=calloc(alloc,1);int ok=rom&&tab5_load(f,rom,size);api->base.close(f);if(!ok)return 4;
 uint32_t crc=tab5_crc32(0,rom,size);tab5_save_name(savepath,"Sega8","sav",crc);tab5_save_name(statepath,"Sega8","state",crc);
 system_reset_config();option.sndrate=22050;option.overscan=0;option.extra_gg=0;option.fm=0;const char* ext=strrchr(argv[1],'.');option.console=ext&&(!strcmp(ext,".gg")||!strcmp(ext,".GG"))?3:1;
 if(!load_rom(rom,alloc,size))return 5;
 bitmap.width=256;bitmap.height=240;bitmap.pitch=256;bitmap.data=calloc(256*240,1);uint16_t* pixels=malloc(256*240*2);int16_t* audio=malloc(2048*2*sizeof(int16_t));if(!bitmap.data||!pixels||!audio)return 6;
 system_poweron();int width=bitmap.viewport.w,height=bitmap.viewport.h;if(width<=0||width>256||height<=0||height>240)return 7;
 FILE* state=fopen(statepath,"rb");if(state){system_load_state(state);fclose(state);}
 if(api->video_begin(width,height))return 6;
 unsigned fraction=0;uint32_t deadline=api->base.millis(),lastSave=deadline;uint16_t palette[256];int skip=0;
 while(!api->base.should_stop()){
  uint32_t k=api->buttons();if(k&256)break;if(k&512)save_game();if(k&1024)system_reset();
  input.pad[0]=((k&4)>>2)|((k&8)>>2)|((k&2)<<1)|((k&1)<<3)|((k&32)>>1)|((k&16)<<1);input.system=k&128?(IS_GG?INPUT_START:INPUT_PAUSE):0;
  system_frame(skip);render_copy_palette(palette);
  if(!skip){for(int y=0;y<height;++y)for(int x=0;x<width;++x){uint16_t c=palette[bitmap.data[(y+bitmap.viewport.y)*256+x+bitmap.viewport.x]&31];pixels[y*width+x]=(c>>8)|(c<<8);}api->video_present(pixels,width*height);}
  int n=snd.sample_count;if(n>2048)n=2048;for(int i=0;i<n;++i){audio[2*i]=snd.stream[0][i];audio[2*i+1]=snd.stream[1][i];}api->audio_pcm(audio,n,22050);
  unsigned fps=sms.display==DISPLAY_NTSC?60:50;fraction+=1000;deadline+=fraction/fps;fraction%=fps;int32_t wait=deadline-api->base.millis();skip=wait<-20;api->base.sleep_ms(wait>0?wait:0);if(wait<-200)deadline=api->base.millis();
  if(api->base.millis()-lastSave>30000){system_manage_sram(cart.sram,0,SRAM_SAVE);lastSave=api->base.millis();}
 }
 save_game();system_poweroff();system_shutdown();return 0;
}
