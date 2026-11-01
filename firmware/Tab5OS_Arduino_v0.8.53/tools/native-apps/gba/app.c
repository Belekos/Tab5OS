#include "../common/runtime.h"
#include "core/common.h"
#include "core/memmap.h"
#include "core/gba_memory.h"
#include "core/gba_cc_lut.h"
#include "bios.h"
#include "core/savestate.h"
u32 idle_loop_target_pc=0xffffffff,translation_gate_target_pc[MAX_TRANSLATION_GATES],translation_gate_targets=0;boot_mode selected_boot_mode=boot_game;u32 skip_next_frame=0;int sprite_limit=1;
static uint32_t pad;static char savepath[80],statepath[80];
void netpacket_poll_receive(void){}void netpacket_send(uint16_t id,const void* p,size_t n){(void)id;(void)p;(void)n;}
void set_fastforward_override(bool value){(void)value;}
int16_t input_cb(unsigned port,unsigned device,unsigned index,unsigned id){(void)port;(void)device;(void)index;(void)id;unsigned out=0;const unsigned bits[]={RETRO_DEVICE_ID_JOYPAD_RIGHT,RETRO_DEVICE_ID_JOYPAD_LEFT,RETRO_DEVICE_ID_JOYPAD_UP,RETRO_DEVICE_ID_JOYPAD_DOWN,RETRO_DEVICE_ID_JOYPAD_A,RETRO_DEVICE_ID_JOYPAD_B,RETRO_DEVICE_ID_JOYPAD_SELECT,RETRO_DEVICE_ID_JOYPAD_START};for(unsigned i=0;i<8;++i)if(pad&(1u<<i))out|=1u<<bits[i];if(pad&(1u<<13))out|=1u<<RETRO_DEVICE_ID_JOYPAD_L;if(pad&(1u<<14))out|=1u<<RETRO_DEVICE_ID_JOYPAD_R;return out;}
static void save_game(void){
 if(tab5_api->save(savepath,gamepak_backup,128*1024))tab5_message("Save failed.","Échec de la sauvegarde.");
 void* data=calloc(1,GBA_STATE_MEM_SIZE);gba_save_state(data);FILE* f=fopen(statepath,"wb");
 if(f){fwrite(data,1,GBA_STATE_MEM_SIZE,f);if(fclose(f))tab5_message("Snapshot failed.","Échec de l'instantané.");}free(data);
}
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){
 const struct tab5_app_api4* api=tab5_app_api_v4();if(!api)return 1;tab5_runtime_init(&api->base.base);if(__builtin_setjmp(tab5_escape))return 8;
 if(argc<2||!argv[1][0]){tab5_message("Choose a .gba ROM.","Choisissez une ROM .gba.");return 2;}int f=tab5_api->base.open_read(argv[1]);if(f<0)return 2;size_t size=tab5_api->file_size(f);uint8_t header[192];if(size<192||size>32u*1024u*1024u||tab5_api->base.read(f,header,sizeof(header))!=sizeof(header)){tab5_api->base.close(f);return 3;}uint32_t crc=tab5_crc32(0,argv[1],strlen(argv[1]));crc=tab5_crc32(crc,header,sizeof(header));tab5_api->base.close(f);
 tab5_runtime_capacity(12u*1024u*1024u);gba_screen_pixels=calloc(1,GBA_SCREEN_BUFFER_SIZE);libretro_supports_bitmasks=true;retro_set_input_state(input_cb);init_gamepak_buffer();init_sound();
 memcpy(bios_rom,open_gba_bios_rom,sizeof(bios_rom));memset(gamepak_backup,0xff,sizeof(gamepak_backup));
 if(load_gamepak(NULL,argv[1],FEAT_DISABLE,FEAT_DISABLE,SERIAL_MODE_DISABLED))return 4;reset_gba();tab5_save_name(savepath,"GBA","sav",crc);f=tab5_api->base.open_read(savepath);if(f>0){if(tab5_api->file_size(f)==128*1024)tab5_load(f,gamepak_backup,128*1024);tab5_api->base.close(f);}
 tab5_save_name(statepath,"GBA","state",crc);FILE* snapshot=fopen(statepath,"rb");if(snapshot){void* data=malloc(GBA_STATE_MEM_SIZE);
 if(fread(data,1,GBA_STATE_MEM_SIZE,snapshot)==GBA_STATE_MEM_SIZE&&!gba_load_state(data))reset_gba();free(data);fclose(snapshot);}
 if(tab5_api->video_begin(240,160))return 5;int16_t* audio=malloc(2048*4);uint32_t deadline=tab5_api->base.millis(),lastSave=deadline,fraction=0;
 while(!tab5_api->base.should_stop()){
  pad=tab5_api->buttons();if(pad&256)break;if(pad&512)save_game();if(pad&1024)reset_gba();update_input();rumble_frame_reset();clear_gamepak_stickybits();execute_arm(execute_cycles);
  if(!skip_next_frame)tab5_api->video_present(gba_screen_pixels,240*160);size_t n=sound_read_samples(audio,2048);tab5_api->audio_pcm(audio,n,GBA_SOUND_FREQUENCY);
  fraction+=1000;deadline+=fraction/60;fraction%=60;int32_t wait=deadline-tab5_api->base.millis();skip_next_frame=wait<-20;tab5_api->base.sleep_ms(wait>0?wait:0);if(wait<-200)deadline=tab5_api->base.millis();if(tab5_api->base.millis()-lastSave>30000){save_game();lastSave=tab5_api->base.millis();}
 }
 save_game();return 0;
}
