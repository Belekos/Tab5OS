#include "Tab5AppApi.h"
#include <string.h>
#define MINIGB_APU_AUDIO_FORMAT_S16SYS
#define ENABLE_SOUND 1
#define PEANUT_GB_12_COLOUR 0
#include "minigb_apu.c"
static struct minigb_apu_ctx apu;
static const struct tab5_app_api2* api;
static int failed=0;
static uint32_t slice_end=0,checks=0;
static int tab5_gb_continue(void){return (++checks&255)||(!failed&&!api->base.should_stop()&&(int32_t)(api->base.millis()-slice_end)<0);}
static uint8_t audio_read(uint16_t address){return minigb_apu_audio_read(&apu,address);}
static void audio_write(uint16_t address,uint8_t value){minigb_apu_audio_write(&apu,address,value);}
#include "peanut_gb.h"
void* memset(void* p,int c,size_t n){unsigned char* b=p;for(size_t i=0;i<n;++i)b[i]=c;return p;}
void* memcpy(void* d,const void* s,size_t n){unsigned char* a=d;const unsigned char* b=s;for(size_t i=0;i<n;++i)a[i]=b[i];return d;}
static uint8_t *rom,*ram;
static size_t rom_size,ram_size;
static uint16_t* pixels;
static int save_dirty;
static uint32_t rom_hash;
static char state_path[40];
static void say(const char* s){size_t n=0;while(s[n])++n;api->base.log(s,n);}
static uint8_t read_rom(struct gb_s* gb,uint_fast32_t a){(void)gb;return a<rom_size?rom[a]:0xff;}
static uint8_t read_ram(struct gb_s* gb,uint_fast32_t a){(void)gb;return a<ram_size?ram[a]:0xff;}
static void write_ram(struct gb_s* gb,uint_fast32_t a,uint8_t b){(void)gb;if(a<ram_size&&ram[a]!=b){ram[a]=b;save_dirty=1;}}
static void fault(struct gb_s* gb,enum gb_error_e e,uint16_t a){(void)gb;(void)e;(void)a;failed=1;say("Unsupported instruction or cartridge access.\n");}
static void line(struct gb_s* gb,const uint8_t* src,const uint_fast8_t y){(void)gb;static const uint16_t palette[]={0xdf59,0x9eaf,0x4bc9,0x19e6};if(y>=144)return;for(unsigned x=0;x<160;++x)pixels[y*160+x]=palette[src[x]&3];}
static int read_all(int f,void* p,size_t n){size_t at=0;while(at<n){size_t want=n-at;if(want>4096)want=4096;int got=api->base.read(f,(uint8_t*)p+at,want);if(got<=0)return 0;at+=got;if(api->base.sleep_ms(0))return 0;}return 1;}

static uint32_t digest(const void* data,size_t size){const uint8_t* p=data;uint32_t h=2166136261u;for(size_t i=0;i<size;++i)h=(h^p[i])*16777619u;return h;}
struct state_header {uint32_t magic,version,rom,core_size,apu_size,ram_size,checksum,reserved;};
static void callbacks(struct gb_s* gb){gb->gb_rom_read=read_rom;gb->gb_cart_ram_read=read_ram;gb->gb_cart_ram_write=write_ram;gb->gb_error=fault;gb->gb_serial_tx=0;gb->gb_serial_rx=0;gb->gb_bootrom_read=0;gb->display.lcd_draw_line=line;gb->direct.priv=0;}
static void snapshot(struct gb_s* gb,int restore){
 size_t bytes=sizeof(struct state_header)+sizeof(*gb)+sizeof(apu)+ram_size;
 uint8_t* buffer=api->base.allocate(bytes);if(!buffer){say("State: insufficient memory.\n");return;}
 struct state_header* head=(struct state_header*)buffer;struct gb_s* saved=(struct gb_s*)(head+1);uint8_t* sound=(uint8_t*)(saved+1);uint8_t* cart=sound+sizeof(apu);
 if(restore){
  int f=api->base.open_read(state_path);if(f<0){char old[48];size_t n=0;while(state_path[n]){old[n]=state_path[n];++n;}memcpy(old+n,".old",5);f=api->base.open_read(old);}
  if(f>0){int ok=api->file_size(f)==bytes&&read_all(f,buffer,bytes);api->base.close(f);
   if(ok&&head->magic==0x35534247&&head->version==0x00008701&&head->rom==rom_hash&&head->core_size==sizeof(*gb)&&head->apu_size==sizeof(apu)&&head->ram_size==ram_size&&head->checksum==digest(head+1,bytes-sizeof(*head))&&saved->mbc==gb->mbc&&saved->num_rom_banks_mask==gb->num_rom_banks_mask&&saved->num_ram_banks==gb->num_ram_banks&&saved->cart_ram==gb->cart_ram&&saved->selected_rom_bank<=gb->num_rom_banks_mask&&saved->cart_ram_bank<16&&saved->hram_io[IO_LY]<154){memcpy(gb,saved,sizeof(*gb));callbacks(gb);memcpy(&apu,sound,sizeof(apu));if(ram_size)memcpy(ram,cart,ram_size);say("Game state restored.\n");}
   else say("State is incompatible or damaged; starting normally.\n");
  }
 }else{
  *head=(struct state_header){0x35534247,0x00008701,rom_hash,sizeof(*gb),sizeof(apu),ram_size,0};memcpy(saved,gb,sizeof(*gb));
  saved->gb_rom_read=0;saved->gb_cart_ram_read=0;saved->gb_cart_ram_write=0;saved->gb_error=0;saved->gb_serial_tx=0;saved->gb_serial_rx=0;saved->gb_bootrom_read=0;saved->display.lcd_draw_line=0;saved->direct.priv=0;
  memcpy(sound,&apu,sizeof(apu));if(ram_size)memcpy(cart,ram,ram_size);head->checksum=digest(head+1,bytes-sizeof(*head));
  say(api->save(state_path,buffer,bytes)?"State save failed.\n":"Game state saved.\n");
 }
 api->base.release(buffer);
}
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){
 api=tab5_app_api_v2();if(!api||api->base.version!=2||api->base.size<sizeof(*api))return 1;
 const char* path=argc>1&&argv[1][0]?argv[1]:"/Games/GB/Tab5_Pocket.gb";
 int f=api->base.open_read(path);if(f<0){say("Open a .gb ROM in Files, or enter its SD path.\n");return 2;}
 rom_size=api->file_size(f);if(rom_size<32768||rom_size>4u*1024u*1024u){say("ROM must be 32 KiB to 4 MiB.\n");api->base.close(f);return 3;}
 rom=api->base.allocate(rom_size);if(!rom){say("Not enough memory for ROM.\n");api->base.close(f);return 4;}
 int ok=read_all(f,rom,rom_size);api->base.close(f);if(!ok)return 5;
 if(rom[0x148]>7||rom[0x149]>5||rom_size!=(32768u<<rom[0x148])||rom[0x143]==0xc0){say("This app supports original Game Boy ROMs, not CGB-only or GBA.\n");return 6;}
 struct gb_s* gb=api->base.allocate(sizeof(*gb));pixels=api->base.allocate(160*144*2);audio_sample_t* samples=api->base.allocate(AUDIO_SAMPLES_TOTAL*sizeof(*samples));
 if(!gb||!pixels||!samples){say("Not enough memory for emulator.\n");return 7;}memset(gb,0,sizeof(*gb));memset(pixels,0,160*144*2);
 minigb_apu_audio_init(&apu);
 if(gb_init(gb,read_rom,read_ram,write_ram,fault,0)!=GB_INIT_NO_ERROR){say("ROM checksum or cartridge type invalid.\n");return 8;}
 if(gb_get_save_size_s(gb,&ram_size)||ram_size>128*1024)return 9;
 uint32_t hash=rom_hash=digest(rom,rom_size);
 char save[32]="/Saves/GB/00000000.sav";const char hex[]="0123456789abcdef";for(unsigned i=0;i<8;++i)save[10+i]=hex[(hash>>(28-4*i))&15];
 if(ram_size){ram=api->base.allocate(ram_size);if(!ram)return 10;memset(ram,0xff,ram_size);f=api->base.open_read(save);if(f<0){char backup[32];size_t n=0;while(save[n]){backup[n]=save[n];++n;}memcpy(backup+n,".old",5);f=api->base.open_read(backup);}if(f>0){if(api->file_size(f)==ram_size)read_all(f,ram,ram_size);api->base.close(f);}}
 memcpy(state_path,save,18);memcpy(state_path+18,".state",7);
 gb_init_lcd(gb,line);snapshot(gb,1);if(api->video_begin(160,144)){say("Cannot open game screen.\n");return 11;}
 say("Game Boy: arrows, Z=A, X=B, Backspace=Select, Enter=Start, Esc=Exit.\n");
 uint32_t start=api->base.millis(),frame=0,last_save=start;int audio_warned=0;
 while(!api->base.should_stop()&&!failed){
  uint32_t keys=api->buttons();if(keys&256)break;
  if(keys&512)snapshot(gb,0);
  if(keys&1024){gb_reset(gb);minigb_apu_audio_init(&apu);say("Game restarted.\n");}
  /* Peanut uses A B Select Start Right Left Up Down (active low). */
  gb->direct.joypad=(uint8_t)~(((keys&15)<<4)|((keys>>4)&15));
  gb->gb_frame=false;slice_end=api->base.millis()+60;checks=0;
  while(!gb->gb_frame&&!failed){
   for(unsigned step=0;step<256&&!gb->gb_frame&&!failed;++step)__gb_step_cpu(gb);
   if(api->base.should_stop()||(int32_t)(api->base.millis()-slice_end)>=0)break;
  }
  if(failed)break;
  if(gb->gb_frame){api->video_present(pixels,160*144);minigb_apu_audio_callback(&apu,samples);if(api->audio_pcm(samples,AUDIO_SAMPLES,AUDIO_SAMPLE_RATE)&&!audio_warned){say("Audio is busy: game continues silently. Stop music to enable game sound.\n");audio_warned=1;}}
  uint32_t now=api->base.millis();
  if(ram_size&&save_dirty&&(uint32_t)(now-last_save)>=30000){if(api->save(save,ram,ram_size)==0)save_dirty=0;else say("Game save failed; will retry.\n");last_save=now;}
  ++frame;uint32_t target=start+(frame*16743u)/1000u;int32_t wait=(int32_t)(target-api->base.millis());
  if(wait>0)api->base.sleep_ms(wait);else {api->base.sleep_ms(0);if(wait< -250){start=api->base.millis();frame=0;}}
  if(frame>60000){start=api->base.millis();frame=0;}
 }
 if(!failed)snapshot(gb,0);
 if(ram_size&&save_dirty&&api->save(save,ram,ram_size))say("Game save failed.\n");
 say("Game closed.\n");return failed?12:0;
}
