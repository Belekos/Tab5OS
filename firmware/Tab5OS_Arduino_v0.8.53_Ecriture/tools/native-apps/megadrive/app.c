#include "../common/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m68k.h"
#include "z80inst.h"
#include "ym2612.h"
#include "gwenesis_bus.h"
#include "gwenesis_io.h"
#include "gwenesis_vdp.h"
#include "gwenesis_savestate.h"
#include "gwenesis_sn76489.h"
unsigned char *M68K_RAM,*ZRAM;int16_t *gwenesis_sn76489_buffer,*gwenesis_ym2612_buffer;
int sn76489_index,sn76489_clock,ym2612_index,ym2612_clock,scan_line;int32_t* lfo_pm_table;signed int* tl_tab;
extern unsigned char *ROM_DATA,*VRAM,*SAT_CACHE,*gwenesis_vdp_regs;extern uint16_t *CRAM,*CRAM565,*VSRAM,*fifo;
extern uint8_t *render_buffer,*sprite_buffer,*OPNREGS;extern unsigned int* sin_tab;extern m68ki_cpu_core* m68k;
extern int zclk,screen_width,screen_height,hint_pending;extern unsigned short gwenesis_vdp_status;
#define AUDIO_BUF_LEN (GWENESIS_AUDIO_BUFFER_LENGTH_PAL*2+128)
void gwenesis_io_get_buttons(void){}
static FILE* stateFile;static char statepath[80];
SaveState* saveGwenesisStateOpenForRead(const char* name){(void)name;return (SaveState*)1;}
SaveState* saveGwenesisStateOpenForWrite(const char* name){(void)name;return (SaveState*)1;}
struct svar {char key[28];uint32_t length;};
void saveGwenesisStateGetBuffer(SaveState* state,const char* tag,void* out,int length){(void)state;if(!stateFile||length<0)abort();fseek(stateFile,4,SEEK_SET);struct svar v;unsigned count=0;while(count++<1024&&fread(&v,sizeof(v),1,stateFile)==1){if(v.length>512*1024)break;if(!strncmp(v.key,tag,28)){if(v.length!=(uint32_t)length||fread(out,1,length,stateFile)!=(size_t)length)abort();return;}if(fseek(stateFile,v.length,SEEK_CUR))break;}abort();}
void saveGwenesisStateSetBuffer(SaveState* state,const char* tag,const void* in,int n){(void)state;if(!stateFile||n<0)return;struct svar v={{0},n};strncpy(v.key,tag,27);fwrite(&v,sizeof(v),1,stateFile);fwrite(in,1,n,stateFile);}
int saveGwenesisStateGet(SaveState* s,const char* t){int n=0;saveGwenesisStateGetBuffer(s,t,&n,sizeof(n));return n;}
void saveGwenesisStateSet(SaveState* s,const char* t,int n){saveGwenesisStateSetBuffer(s,t,&n,sizeof(n));}
static void save_game(void){stateFile=fopen(statepath,"wb");if(stateFile){uint32_t version=0x4d443032;fwrite(&version,4,1,stateFile);gwenesis_save_state();if(fclose(stateFile))tab5_message("Save failed.","Échec de la sauvegarde.");stateFile=NULL;}}
static void genesis_alloc_core(void)
{
    /* M68K CPU state — hottest structure, accessed every opcode → internal RAM */
    m68k = (m68ki_cpu_core *)calloc(1, sizeof(m68ki_cpu_core));

    /* M68K RAM — 64 KB — every RAM read/write → internal RAM */
    M68K_RAM = (uint8_t *)calloc(1, MAX_RAM_SIZE);

    /* Z80 RAM — 8 KB → internal RAM */
    ZRAM = (uint8_t *)calloc(1, MAX_Z80_RAM_SIZE);

    /* VRAM — 64 KB → internal RAM (VDP rendering hot path) */
    VRAM = (uint8_t *)calloc(1, VRAM_MAX_SIZE);
    if (!VRAM) {
        
        VRAM = (uint8_t *)calloc(1, VRAM_MAX_SIZE);
    }

    /* CRAM, VSRAM, SAT cache, VDP regs, FIFO — small, hot → internal RAM */
    CRAM      = (uint16_t *)calloc(1, CRAM_MAX_SIZE * sizeof(uint16_t));
    CRAM565   = (uint16_t *)calloc(1, CRAM_MAX_SIZE * 4 * sizeof(uint16_t));
    VSRAM     = (uint16_t *)calloc(1, VSRAM_MAX_SIZE * sizeof(uint16_t));
    SAT_CACHE = (uint8_t *)calloc(1, SAT_CACHE_MAX_SIZE);
    gwenesis_vdp_regs = (uint8_t *)calloc(1, REG_SIZE);
    fifo = (uint16_t *)calloc(1, FIFO_SIZE * sizeof(uint16_t));

    /* Render / sprite line buffers — per-pixel writes during rendering → internal RAM */
    render_buffer = (uint8_t *)calloc(1, SCREEN_WIDTH + PIX_OVERFLOW * 2);
    sprite_buffer = (uint8_t *)calloc(1, SCREEN_WIDTH + PIX_OVERFLOW * 2);

    /* YM2612 state — hot during per-scanline synthesis → internal RAM */
    ym2612 = (YM2612 *)calloc(1, sizeof(YM2612));
    if (!ym2612) {
        
        ym2612 = (YM2612 *)calloc(1, sizeof(YM2612));
    }
    OPNREGS = (uint8_t *)calloc(1, 512);
    sin_tab = (unsigned int *)calloc(1, SIN_LEN * sizeof(unsigned int));
    tl_tab = (signed int *)calloc(1, 13 * 2 * 256 * sizeof(signed int));
    if (!tl_tab) {
        
        tl_tab = (signed int *)calloc(1, 13 * 2 * 256 * sizeof(signed int));
    }

    /* LFO PM table — 128 KB, PSRAM */
    lfo_pm_table = (int32_t *)calloc(1, 128 * 8 * 32 * sizeof(int32_t));

    /* Audio buffers — internal RAM to avoid PSRAM jitter during mixing */
    gwenesis_sn76489_buffer = (int16_t *)calloc(1, AUDIO_BUF_LEN * sizeof(int16_t));
    gwenesis_ym2612_buffer  = (int16_t *)calloc(1, AUDIO_BUF_LEN * sizeof(int16_t));

    
    
}

__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){
 const struct tab5_app_api4* api=tab5_app_api_v4();if(!api)return 1;tab5_runtime_init(&api->base.base);if(__builtin_setjmp(tab5_escape))return 9;
 if(argc<2||!argv[1][0]){tab5_message("Choose a .md or .gen ROM.","Choisissez une ROM .md ou .gen.");return 2;}int f=tab5_api->base.open_read(argv[1]);if(f<0)return 2;size_t bytes=tab5_api->file_size(f);if(bytes<512||bytes>8*1024*1024||(bytes&1)){tab5_api->base.close(f);return 3;}
 tab5_runtime_capacity(bytes+3u*1024u*1024u);genesis_alloc_core();ROM_DATA=calloc((bytes+65535)&~65535u,1);if(!tab5_load(f,ROM_DATA,bytes)){tab5_api->base.close(f);return 4;}tab5_api->base.close(f);
 if(ROM_DATA[1]==3&&ROM_DATA[8]==0xaa&&ROM_DATA[9]==0xbb){tab5_message("Use an uncompressed, non-interleaved .md ROM.","Utilisez une ROM .md non compressée et non entrelacée.");return 5;}
 uint32_t crc=tab5_crc32(0,ROM_DATA,bytes);tab5_save_name(statepath,"MegaDrive","state",crc);load_cartridge(ROM_DATA,bytes);power_on();reset_emulation();
 uint8_t* frame=calloc(320*240,1);uint16_t* pixels=calloc(320*240,2);int16_t* audio=malloc(2048*4);gwenesis_vdp_set_buffer(frame);stateFile=fopen(statepath,"rb");if(stateFile){uint32_t version=0;if(fread(&version,4,1,stateFile)==1&&version==0x4d443032)gwenesis_load_state();fclose(stateFile);stateFile=NULL;}
 if(tab5_api->video_begin(320,240))return 6;uint32_t deadline=tab5_api->base.millis(),fraction=0;int skip=0;
 while(!tab5_api->base.should_stop()){
  uint32_t k=tab5_api->buttons();if(k&256)break;if(k&512)save_game();if(k&1024)reset_emulation();for(int i=0;i<=PAD_S;++i)gwenesis_io_pad_release_button(0,i);
  const unsigned pads[]={PAD_RIGHT,PAD_LEFT,PAD_UP,PAD_DOWN,PAD_B,PAD_C};for(unsigned i=0;i<6;++i)if(k&(1u<<i))gwenesis_io_pad_press_button(0,pads[i]);if(k&128)gwenesis_io_pad_press_button(0,PAD_S);if(k&(1u<<11))gwenesis_io_pad_press_button(0,PAD_A);
  int lines=REG1_PAL?LINES_PER_FRAME_PAL:LINES_PER_FRAME_NTSC,hint=gwenesis_vdp_regs[10];screen_width=REG12_MODE_H40?320:256;screen_height=REG1_PAL?240:224;gwenesis_vdp_render_config();int clock=0;zclk=ym2612_clock=sn76489_clock=ym2612_index=sn76489_index=0;
  for(scan_line=0;scan_line<lines;){clock+=VDP_CYCLES_PER_LINE;m68k_run(clock);z80_run(clock);ym2612_run(clock);gwenesis_SN76489_run(clock);if(!skip&&scan_line<screen_height)gwenesis_vdp_render_line(scan_line);
   if(scan_line==0||scan_line>screen_height)hint=REG10_LINE_COUNTER;if(--hint<0){if(REG0_LINE_INTERRUPT&&scan_line<=screen_height){hint_pending=1;if(!(gwenesis_vdp_status&STATUS_VIRQPENDING))m68k_update_irq(4);}hint=REG10_LINE_COUNTER;}
   ++scan_line;if(scan_line==screen_height){if(REG1_VBLANK_INTERRUPT){gwenesis_vdp_status|=STATUS_VIRQPENDING;m68k_set_irq(6);}z80_irq_line(1);}if(scan_line==screen_height+1)z80_irq_line(0);
  }
  if(!skip){memset(pixels,0,320*240*2);for(int y=0;y<screen_height;++y)for(int x=0;x<screen_width;++x)pixels[y*320+x+(320-screen_width)/2]=CRAM565[frame[y*320+x]];tab5_api->video_present(pixels,320*240);}
  int n=ym2612_index;if(n>sn76489_index)n=sn76489_index;if(n>AUDIO_BUF_LEN)n=AUDIO_BUF_LEN;unsigned frames=0;
  for(int i=0;i+1<n;i+=2){int v=(gwenesis_ym2612_buffer[i]+gwenesis_ym2612_buffer[i+1]+gwenesis_sn76489_buffer[i]+gwenesis_sn76489_buffer[i+1])/2;
   if(v>32767)v=32767;if(v<-32768)v=-32768;audio[frames*2]=audio[frames*2+1]=v;++frames;}
  if(frames)tab5_api->audio_pcm(audio,frames,(REG1_PAL?GWENESIS_AUDIO_FREQ_PAL:GWENESIS_AUDIO_FREQ_NTSC)/2);
  unsigned fps=REG1_PAL?50:60;fraction+=1000;deadline+=fraction/fps;fraction%=fps;int32_t wait=deadline-tab5_api->base.millis();skip=wait<-20;tab5_api->base.sleep_ms(wait>0?wait:0);if(wait<-200)deadline=tab5_api->base.millis();
 }
 save_game();return 0;
}
