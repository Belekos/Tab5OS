#include "Tab5AppApi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core/gnuboy.h"
#include "core/hw.h"
static const struct tab5_app_api2* api;
static uint32_t romhash,frame_deadline;
static int io_error;
static int french;
static void say(const char* en,const char* fr){const char* s=french?fr:en;api->base.log(s,strlen(s));}
void* memset(void* p,int c,size_t n){unsigned char* d=p;while(n--)*d++=(unsigned char)c;return p;}
void* memcpy(void* p,const void* s,size_t n){unsigned char* d=p;const unsigned char* b=s;while(n--)*d++=*b++;return p;}
int memcmp(const void* a,const void* b,size_t n){const unsigned char* x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;++x;++y;}return 0;}
size_t strlen(const char* s){size_t n=0;while(s[n])++n;return n;}
int strcmp(const char* a,const char* b){while(*a&&*a==*b){++a;++b;}return (unsigned char)*a-(unsigned char)*b;}
int strncmp(const char* a,const char* b,size_t n){while(n--){if(*a!=*b||!*a)return (unsigned char)*a-(unsigned char)*b;++a;++b;}return 0;}
void* malloc(size_t n){return api->base.allocate(n);}
void free(void* p){api->base.release(p);}
void* calloc(size_t n,size_t m){if(m&&n>SIZE_MAX/m)return NULL;void* p=malloc(n*m);if(p)memset(p,0,n*m);return p;}
static uint32_t hash(const void* data,size_t n){const uint8_t* p=data;uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
static int read_all(int f,void* dest,size_t n){uint8_t* p=dest;while(n){size_t k=n>4096?4096:n;int r=api->base.read(f,p,k);if(r!=(int)k)return 0;p+=k;n-=k;if(api->base.sleep_ms(0))return 0;}return 1;}
#define SAVE_CAP (192u*1024u)
struct file {char path[80];uint8_t* bytes;size_t pos,size,capacity;int write,state,error;};
struct header {uint32_t magic,rom,size,checksum;};
FILE* fopen(const char* path,const char* mode){
 size_t n=strlen(path);if(n>=80)return NULL;
 struct file* f=calloc(1,sizeof(*f));if(!f)return NULL;
 memcpy(f->path,path,n+1);f->write=mode[0]=='w';f->state=n>=6&&!strcmp(path+n-6,".state");
 if(f->write){f->capacity=SAVE_CAP;f->bytes=calloc(1,SAVE_CAP+sizeof(struct header));if(!f->bytes){free(f);return NULL;}return (FILE*)f;}
 int fd=api->base.open_read(path);if(fd<0){free(f);return NULL;}
 size_t bytes=api->file_size(fd);if(!bytes||bytes>SAVE_CAP+sizeof(struct header)){api->base.close(fd);free(f);return NULL;}
 f->bytes=malloc(bytes);int ok=f->bytes&&read_all(fd,f->bytes,bytes);api->base.close(fd);
 if(ok&&f->state){struct header* h=(struct header*)f->bytes;ok=bytes>=sizeof(*h)&&h->magic==0x39434247&&h->rom==romhash&&h->size==bytes-sizeof(*h)&&h->checksum==hash(h+1,h->size);if(ok){f->size=h->size;for(size_t i=0;i<f->size;++i)f->bytes[i]=f->bytes[sizeof(*h)+i];}}
 else f->size=bytes;
 if(!ok){free(f->bytes);free(f);return NULL;}return (FILE*)f;
}
size_t fread(void* ptr,size_t size,size_t count,FILE* stream){struct file* f=(struct file*)stream;if(!f||f->write||!size||count>SIZE_MAX/size)return 0;size_t n=size*count;if(n>f->size-f->pos)n=f->size-f->pos;memcpy(ptr,f->bytes+f->pos,n);f->pos+=n;return n/size;}
size_t fwrite(const void* ptr,size_t size,size_t count,FILE* stream){struct file* f=(struct file*)stream;if(!f||!f->write||!size||count>SIZE_MAX/size)return 0;size_t n=size*count;if(n>f->capacity-f->pos){f->error=1;return 0;}memcpy(f->bytes+f->pos,ptr,n);f->pos+=n;if(f->pos>f->size)f->size=f->pos;return count;}
int fseek(FILE* stream,long offset,int whence){struct file* f=(struct file*)stream;if(!f)return -1;long p=offset+(whence==SEEK_CUR?f->pos:whence==SEEK_END?f->size:0);if(p<0||(size_t)p>(f->write?f->capacity:f->size))return -1;f->pos=p;return 0;}
int fclose(FILE* stream){struct file* f=(struct file*)stream;if(!f)return 0;int result=f->error?-1:0;if(f->write&&!result){size_t size=f->size;if(f->state){for(size_t i=size;i;--i)f->bytes[sizeof(struct header)+i-1]=f->bytes[i-1];struct header* h=(struct header*)f->bytes;*h=(struct header){0x39434247,romhash,size,hash(h+1,size)};size+=sizeof(*h);}result=size>SAVE_CAP?-1:api->save(f->path,f->bytes,size);}if(result)io_error=1;free(f->bytes);free(f);return result;}
int feof(FILE* stream){struct file* f=(struct file*)stream;return !f||f->pos>=f->size;}
int tab5_gbc_continue(void){return !api->base.should_stop()&&(int32_t)(api->base.millis()-frame_deadline)<0;}
static void video(void* pixels){api->video_present(pixels,160u*144u);}
static void audio(void* samples,size_t count){api->audio_pcm(samples,count/2,22050);}
static void save_game(const char* path,const char* state){io_error=0;if(cart.has_battery)gnuboy_save_sram(path,false);int r=gnuboy_save_state(state);say((r||io_error)?"Save failed.\n":"Game saved.\n",(r||io_error)?"Échec de la sauvegarde.\n":"Partie sauvegardée.\n");}
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){
 api=tab5_app_api_v2();if(!api||api->base.version!=2||api->base.size<sizeof(*api))return 1;french=api->base.language[0]=='f';
 const char* path=argc>1&&argv[1][0]?argv[1]:"/Games/GBC/Tab5_Color.gbc";
 int fd=api->base.open_read(path);if(fd<0){say("Choose a .gbc or .gb ROM.\n","Choisissez une ROM .gbc ou .gb.\n");return 2;}
 size_t size=api->file_size(fd);if(size<32768||size>4u*1024u*1024u){api->base.close(fd);say("ROM size: 32 KiB to 4 MiB.\n","Taille de ROM : 32 Kio à 4 Mio.\n");return 3;}
 uint8_t* rom=malloc(size);int ok=rom&&read_all(fd,rom,size);api->base.close(fd);if(!ok)return 4;
 if(rom[0x148]>7||size!=(32768u<<rom[0x148])||rom[0x149]>5){say("Unsupported or incomplete cartridge.\n","Cartouche incompatible ou incomplète.\n");return 5;}
 unsigned mbc=rom[0x147];if(mbc!=0&&mbc!=8&&mbc!=9&&!(mbc>=1&&mbc<=3)&&mbc!=5&&mbc!=6&&!(mbc>=15&&mbc<=19)&&!(mbc>=25&&mbc<=30)){say("Cartridge controller not supported.\n","Contrôleur de cartouche non pris en charge.\n");return 5;}
 romhash=hash(rom,size);
 if(gnuboy_init(22050,GB_AUDIO_STEREO_S16,GB_PIXEL_565_LE,video,audio)||gnuboy_load_rom(rom,size))return 6;
 uint16_t* pixels=calloc(160u*144u,2);int16_t* samples=calloc(2048,2);if(!pixels||!samples)return 7;
 gnuboy_set_framebuffer(pixels);gnuboy_set_soundbuffer(samples,2048);gnuboy_reset(true);
 char save[48]="/Saves/GBC/00000000.sav",state[48]="/Saves/GBC/00000000.state";const char* hex="0123456789abcdef";for(unsigned i=0;i<8;++i)save[11+i]=state[11+i]=hex[(romhash>>(28-4*i))&15];
 gnuboy_load_sram(save);if(!gnuboy_load_state(state))say("Game resumed.\n","Partie reprise.\n");
 if(api->video_begin(160,144))return 8;
 say("Game Boy Color — arrows, Z/A, X/B, Enter, Backspace, Esc.\n","Game Boy Color — flèches, Z/A, X/B, Entrée, Retour arrière, Échap.\n");
 uint32_t start=api->base.millis(),frames=0,lastSave=start;
 while(!api->base.should_stop()){
  uint32_t keys=api->buttons();if(keys&256)break;if(keys&512)save_game(save,state);if(keys&1024)gnuboy_reset(false);
  gnuboy_set_pad(keys&255);frame_deadline=api->base.millis()+50;gnuboy_run(true);
  uint32_t now=api->base.millis();if(now-lastSave>30000&&gnuboy_sram_dirty()){io_error=0;gnuboy_save_sram(save,false);lastSave=now;}
  uint32_t target=start+(++frames*16743u)/1000u;int32_t wait=(int32_t)(target-api->base.millis());if(wait>0)api->base.sleep_ms(wait);else {api->base.sleep_ms(0);if(wait<-200){frames=0;start=api->base.millis();}}
 }
 save_game(save,state);return 0;
}

#ifndef TAB5_HOST_TEST
#undef memset
void* memset(void* p,int c,size_t n){return tab5_gbc_memset(p,c,n);}
#undef memcpy
void* memcpy(void* p,const void* s,size_t n){return tab5_gbc_memcpy(p,s,n);}
#endif
