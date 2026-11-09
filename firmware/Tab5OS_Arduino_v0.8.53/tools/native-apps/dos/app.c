/* Tab5OS PC/AT: a real 80286 + BIOS + FreeDOS in an isolated application.
 * All hardware access goes through SDK callbacks. Guest files are read-only
 * bases; changed sectors use a bounded, explicitly saved copy-on-write layer. */
#include "platform.h"
#include "../common/runtime.h"
#include "pc_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "app_mouse.h"
#include "presentation.h"
#include "pacing.h"
#include "app_audio.h"
TAB5_APP_STACK(48u*1024u);
#define W 800
#define H 480
static const struct tab5_app_api *base;
static const struct tab5_app_api6 *canvas;
static uint16_t *pixels,*line;
static int keyboard_visible,quit,usb_connected,shift,control,alt;
static uint16_t held_touch;
static char notice[96],program[13];
static struct tab5_app_keyboard_state old_keys;
static uint8_t input_sources[256];
static int ui_dirty=1;
static char performance[40];
int dos_french_keyboard;
uint8_t *vga_planes(void);
bool dos_keyboard_typing(void);
#include "startup.h"
extern const uint8_t pc_font_bios8[1024];
static void message(const char *s){snprintf(notice,sizeof(notice),"%s",s);ui_dirty=1;puts(s);}
static void rect(int x,int y,int w,int h,uint16_t color){for(int yy=y;yy<y+h&&yy<H;yy++)for(int xx=x;xx<x+w&&xx<W;xx++)if(xx>=0&&yy>=0)pixels[yy*W+xx]=color;}
static void text(int x,int y,const char *s,uint16_t color){for(;*s&&x<W;s++,x+=8){unsigned c=(unsigned char)*s;if(c>=128)c='?';for(int row=0;row<8;row++)for(int col=0;col<8;col++)if(pc_font_bios8[c*8+row]&(128>>col))rect(x+col,y+row,1,1,color);}}
struct hit {int x,y,w,h;uint16_t code;char ch;int action;};
static struct hit hits[100];static int hit_count;
enum {ACT_NONE,ACT_SAVE,ACT_KEYS,ACT_EXIT,ACT_LAYOUT,ACT_SHIFT,ACT_CTRL,ACT_ALT,ACT_VOL_DOWN,ACT_VOL_UP,ACT_MUTE};
static void button(int x,int y,int w,int h,const char *label,int action,uint16_t code,char ch,int active){
 rect(x,y,w,h,active?0x126f:0x2945);rect(x,y,w,1,0x630c);text(x+4,y+(h-8)/2,label,0xffff);
 if(hit_count<100)hits[hit_count++]=(struct hit){x,y,w,h,code,ch,action};
}
static int screen_contains(const char *word){
 if(pc.ram[0x449]!=3)return 0;size_t len=strlen(word);uint8_t *v=vga_planes();
 for(unsigned i=0;i+len<=2000;i++){size_t j=0;while(j<len&&v[(i+j)*8]==(unsigned char)word[j])j++;if(j==len)return 1;}return 0;
}
uint16_t dos_ascii_key(unsigned char ch){
 static const char letters[]="azertyuiopqsdfghjklmwxcvbn";
 static const uint8_t scans[]={16,17,18,19,20,21,22,23,24,25,30,31,32,33,34,35,36,37,38,39,44,45,46,47,48,49};
 int upper=ch>='A'&&ch<='Z';unsigned char lower=(unsigned char)tolower(ch);
 for(unsigned i=0;i<26;i++)if(letters[i]==lower)return (uint16_t)(scans[i]|(upper?0x100:0));
 if(ch>='1'&&ch<='9')return (uint16_t)(0x100+2+ch-'1');if(ch=='0')return 0x10b;
 switch(ch){case 27:return 1;case '\b':return 14;case '\t':return 15;case '\n':case '\r':return 28;case ' ':return 57;
 case '&':return 2;case '"':return 4;case '\'':return 5;case '(':return 6;case '-':return 7;case '_':return 9;case ')':return 12;case '=':return 13;case '+':return 0x10d;
 case '$':return 27;case '*':return 43;case ',':return 50;case '?':return 0x132;case ';':return 51;case '.':return 0x133;case ':':return 52;case '/':return 0x134;case '!':return 53;case '%':return 0x128;
 case '~':return 0x203;case '#':return 0x204;case '{':return 0x205;case '[':return 0x206;case '|':return 0x207;case '`':return 0x208;case '\\':return 0x209;case '^':return 0x20a;case '@':return 0x20b;case ']':return 0x20c;case '}':return 0x20d;default:return 0;}
}
static uint16_t hid_scan(unsigned usage){
 static const uint8_t letters[]={30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44};
 if(usage>=4&&usage<=29)return letters[usage-4];if(usage>=30&&usage<=38)return (uint16_t)(usage-28);if(usage==39)return 11;
 if(usage>=58&&usage<=67)return (uint16_t)(59+usage-58);
 switch(usage){case 40:return 28;case 41:return 1;case 42:return 14;case 43:return 15;case 44:return 57;case 45:return 12;case 46:return 13;case 47:return 26;case 48:return 27;case 49:case 50:return 43;case 51:return 39;case 52:return 40;case 53:return 41;case 54:return 51;case 55:return 52;case 56:return 53;case 57:return 58;case 68:return 87;case 69:return 88;case 70:return 0xe037;case 71:return 70;case 73:return 0xe052;case 74:return 0xe047;case 75:return 0xe049;case 76:return 0xe053;case 77:return 0xe04f;case 78:return 0xe051;case 79:return 0xe04d;case 80:return 0xe04b;case 81:return 0xe050;case 82:return 0xe048;case 83:return 69;case 84:return 0xe035;case 85:return 55;case 86:return 74;case 87:return 78;case 88:return 0xe01c;case 89:return 79;case 90:return 80;case 91:return 81;case 92:return 75;case 93:return 76;case 94:return 77;case 95:return 71;case 96:return 72;case 97:return 73;case 98:return 82;case 99:return 83;case 100:return 86;default:return 0;}
}
static void input_key(uint16_t scan,int down,unsigned source){
 unsigned index=(scan&0x7f)+((scan&0xff00)?128u:0u);uint8_t before=input_sources[index];
 if(down)input_sources[index]|=(uint8_t)source;else input_sources[index]&=(uint8_t)~source;
 if(!before&&input_sources[index])pc_key(scan,true);else if(before&&!input_sources[index])pc_key(scan,false);
}
int dos_key_held(uint16_t scan){return input_sources[(scan&0x7f)+((scan&0xff00)?128u:0u)]!=0;}
static void physical_keyboard(void){
 struct tab5_app_keyboard_state now={0};now.size=sizeof(now);if(dos_api->keyboard_state(&now)<0)return;usb_connected=now.connected;
 static const uint16_t modifiers[]={29,42,56,0,0xe01d,54,0xe038,0};
 for(unsigned i=0;i<8;i++)if(((now.modifiers^old_keys.modifiers)>>i)&1u)if(modifiers[i])input_key(modifiers[i],(now.modifiers>>i)&1u,1);
 for(unsigned u=4;u<128;u++)if((now.keys[u/8]^old_keys.keys[u/8])&(1u<<(u%8))){uint16_t scan=hid_scan(u);if(scan)input_key(scan,(now.keys[u/8]>>(u%8))&1u,1);}
 old_keys=now;
}
static int layout_pending=-1;static uint32_t layout_wait,layout_vector;
static uint32_t keyboard_vector(void){
 /* KEYB hooks the BIOS keyboard filter and DOS multiplex interrupt; it need
    not replace IRQ1 itself. Observe all keyboard-related resident vectors. */
 static const uint8_t vectors[]={9,0x15,0x16,0x2f};uint32_t h=2166136261u;
 for(unsigned i=0;i<sizeof(vectors);i++)for(unsigned j=0;j<4;j++)h=(h^pc.ram[vectors[i]*4+j])*16777619u;
 return h;
}
static void change_layout(void){
 if(layout_pending>=0||dos_keyboard_typing())return;
 if(dos_french_keyboard)pc_type("keyb /u\n");else pc_type("keyb fr,437,a:\\keyboard.sys\n");
 layout_pending=!dos_french_keyboard;layout_wait=0;layout_vector=keyboard_vector();message("Changing DOS keyboard layout...");
}
static void action(int a){ui_dirty=1;switch(a){
 case ACT_SAVE:message(dos_overlay_save()?"Save failed; changes remain in this session.":"DOS changes saved.");break;
 case ACT_KEYS:keyboard_visible=!keyboard_visible;break;
 case ACT_EXIT:if(dos_overlay_save())message("Save failed. Session kept open; retry Save or force stop to discard.");else quit=1;break;
 case ACT_LAYOUT:change_layout();break;
 case ACT_VOL_DOWN:dos_audio_change(-10);break;case ACT_VOL_UP:dos_audio_change(10);break;case ACT_MUTE:dos_audio_mute();break;
 case ACT_SHIFT:shift=!shift;break;case ACT_CTRL:control=!control;input_key(29,control,4);break;case ACT_ALT:alt=!alt;input_key(56,alt,4);break;default:break;}}
static void release_touch(void){if(held_touch){input_key(held_touch,false,2);held_touch=0;}}
static int pointer_control(int x,int y){
 for(int i=0;i<hit_count;i++){struct hit *h=&hits[i];if(x<h->x||y<h->y||x>=h->x+h->w||y>=h->y+h->h)continue;
  if(h->action)action(h->action);else if(h->ch){char value[2]={shift?(char)toupper(h->ch):h->ch,0};pc_type(value);}
  else if(h->code){held_touch=h->code;input_key(h->code,true,2);}return 1;
 }return 0;
}
static void input(void){
 static unsigned mouse_buttons;static int mouse_control;
 physical_keyboard();struct tab5_app_input_event e={0};e.size=sizeof(e);
 for(int n=0;n<64&&canvas->input_poll(&e)>0;n++){
  if(e.type==TAB5_INPUT_CLOSE){dos_pointer_cancel();action(ACT_EXIT);continue;}
  if(e.type==TAB5_INPUT_POINTER_POSITION){
   if(!(e.key&1u)&&mouse_control){release_touch();mouse_control=0;}
   if((e.key&1u)&&!(mouse_buttons&1u)){release_touch();mouse_control=pointer_control(e.x,e.y);}
   mouse_buttons=e.key;dos_pointer_mouse_position(e.x,e.y,mouse_control?0:e.key);continue;
  }
  if(e.type==TAB5_INPUT_POINTER_RELATIVE){dos_pointer_mouse_relative(e.x,e.y);continue;}
  /* USB primary clicks also produce legacy canvas events for older apps.
     The richer pointer pair above is authoritative for DOS. */
  if(e.modifiers&TAB5_POINTER_MOUSE)continue;
  if(e.type==TAB5_INPUT_CANCEL){release_touch();dos_pointer_cancel();mouse_buttons=0;mouse_control=0;continue;}
  if(e.type==TAB5_INPUT_UP){release_touch();dos_pointer_touch_up(base->millis());continue;}
  if(e.type==TAB5_INPUT_MOVE){dos_pointer_touch_move(e.x,e.y);continue;}
  if(e.type==TAB5_INPUT_DOWN){release_touch();if(!pointer_control(e.x,e.y))dos_pointer_touch_down(e.x,e.y,base->millis());}
  /* Physical keyboards use the held HID snapshot. Queued OS text is ignored,
     including on disconnect, so no key is replayed or injected twice. */
 }
 dos_pointer_tick(base->millis());
}
static int render(void){
 static uint32_t last_signature;static int last_w,last_h,last_keyboard=-1,last_dirty=-1;
 int w,h;uint32_t signature=pc_screen_signature();pc_screen_size(&w,&h);
 int layout=last_w!=w||last_h!=h||last_keyboard!=keyboard_visible;
 int changed=layout||last_signature!=signature;
 int dirty=dos_overlay_dirty();
 if(!changed&&!ui_dirty&&dirty==last_dirty)return 0;
 if(layout)memset(pixels,0,W*H*2);
 else {rect(0,0,W,36,0);rect(0,436,W,H-436,0);}
 int available=keyboard_visible?252:400;
 if(w>0&&w<=1024&&h>0&&h<=768){int dw=w,dh=h;if(dw>800){dh=dh*800/dw;dw=800;}if(dh>available){dw=dw*available/dh;dh=available;}int x0=(800-dw)/2,y0=36+(available-dh)/2;
  dos_pointer_viewport(x0,y0,dw,dh,w,h);
  if(changed)dos_present_screen(pixels+y0*W+x0,W,w,h,dw,dh,line,pc_render_line_rgb565);}
 hit_count=0;button(4,3,110,28,"DOS 286",0,0,0,0);button(120,3,100,28,"Save",ACT_SAVE,0,0,dos_overlay_dirty());button(226,3,100,28,"Keys",ACT_KEYS,0,0,keyboard_visible);button(332,3,170,28,dos_french_keyboard?"Keyboard: FR":"Keyboard: US",ACT_LAYOUT,0,0,0);button(684,3,110,28,"Save & exit",ACT_EXIT,0,0,0);
 char volume[12];snprintf(volume,sizeof(volume),dos_audio_failed()?"ERR":"%d%%",dos_audio_volume());
 button(508,3,48,28,"Vol-",ACT_VOL_DOWN,0,0,0);button(558,3,66,28,volume,ACT_MUTE,0,0,!dos_audio_volume());button(626,3,50,28,"Vol+",ACT_VOL_UP,0,0,0);
 if(keyboard_visible){
  button(4,294,48,28,"Esc",0,1,0,0);for(int i=0;i<12;i++){char f[5];snprintf(f,sizeof(f),"F%d",i+1);button(56+i*45,294,43,28,f,0,(uint16_t)(i<10?59+i:87+i-10),0,0);}button(602,294,62,28,"Tab",0,15,0,0);button(670,294,124,28,"Backspace",0,14,0,0);
  const char *rows[]={"1234567890-=/\\",dos_french_keyboard?"azertyuiop[]":"qwertyuiop[]",dos_french_keyboard?"qsdfghjklm;:'":"asdfghjkl;:'",dos_french_keyboard?"wxcvbn,.!?":"zxcvbnm,.!?"};
  for(int y=0;y<4;y++)for(int x=0;rows[y][x];x++){char label[2]={shift?(char)toupper(rows[y][x]):rows[y][x],0};button(4+x*43,326+y*28,41,26,label,0,0,rows[y][x],0);}
  button(610,326,88,54,"Enter",0,28,0,0);button(704,326,90,26,"Delete",0,0xe053,0,0);button(704,354,90,26,"Up",0,0xe048,0,0);
  button(610,382,58,26,"Left",0,0xe04b,0,0);button(670,382,58,26,"Down",0,0xe050,0,0);button(730,382,64,26,"Right",0,0xe04d,0,0);
  button(4,440,78,28,"Shift",ACT_SHIFT,0,0,shift);button(88,440,72,28,"Ctrl",ACT_CTRL,0,0,control);button(166,440,72,28,"Alt",ACT_ALT,0,0,alt);button(244,440,300,28,"Space",0,0,' ',0);button(550,440,110,28,"Home",0,0xe047,0,0);button(666,440,128,28,"End",0,0xe04f,0,0);
 }else {text(8,450,notice,0xbdf7);text(544,468,performance,0x7bef);}
 tab5_api->video_present(pixels,W*H);
 last_w=w;last_h=h;last_keyboard=keyboard_visible;last_signature=signature;last_dirty=dirty;ui_dirty=0;return 1;
}
static int executable(const char *path){
 const char *ext=strrchr(path,'.');if(!ext)return 0;
 int com=!strncasecmp(ext,".com",5),exe=!strncasecmp(ext,".exe",5);if(!com&&!exe)return 0;
 int h=base->open_read(path);if(h<0)return 0;uint32_t bytes=tab5_api->file_size(h);uint8_t header[64]={0};int n=base->read(h,header,sizeof(header));int ok=com?bytes>0&&bytes<=65280:n>=28&&header[0]=='M'&&header[1]=='Z';
 if(ok&&exe&&n>=64){uint32_t off=(uint32_t)header[60]|(uint32_t)header[61]<<8|(uint32_t)header[62]<<16|(uint32_t)header[63]<<24;if(off>=28&&off<=bytes-4){uint8_t sig[4];if(!base->seek(h,off)&&base->read(h,sig,4)==4&&((sig[0]=='P'&&sig[1]=='E')||(sig[0]=='N'&&sig[1]=='E')||(sig[0]=='L'&&(sig[1]=='E'||sig[1]=='X'))))ok=0;}}
 base->close(h);return ok;
}
int tab5_app_main(int argc,char **argv){
 dos_api=tab5_app_api_v7();if(!dos_api)return -1;canvas=&dos_api->base;
 const struct tab5_app_api2 *video=&canvas->base.base.base.base;
 if(video->base.version<7||video->base.size<sizeof(*dos_api)||!dos_api->keyboard_state||!dos_api->directory_open||!dos_api->directory_next||!canvas->canvas_begin||!canvas->input_poll)return -1;
 tab5_runtime_init(video);base=&tab5_api->base;
 tab5_runtime_capacity(8u*1024u*1024u);if(__builtin_setjmp(tab5_escape)){base->log("DOS stopped after an allocation or runtime error.\n",49);return -1;}
 const char *arg=argc>1?argv[1]:NULL;const char *boot="/DOS/FREEDOS.IMG";
 snprintf(dos_mount_root,sizeof(dos_mount_root),"/DOS");
 if(arg&&*arg){if(strlen(arg)>=256||arg[0]!='/'||!strcmp(arg,"/USB")||!strncmp(arg,"/USB/",5)){message("DOS requires a file on the SD card.");return -1;}
  if(!executable(arg)){message("Choose a 16-bit DOS .COM or MZ .EXE; Windows programs are unsupported.");return -1;}
  strcpy(dos_mount_root,arg);char *slash=strrchr(dos_mount_root,'/');if(slash==dos_mount_root)slash[1]=0;else *slash=0;
 }
 message("Booting FreeDOS; keyboard and 16-bit programs use a PC/AT 286.");
 pc_config_t cfg={0};cfg.board=PC_AT;cfg.cpu=X86_80286;cfg.cpu_hz=6000000;cfg.video=PC_VGA;cfg.conv_kb=640;cfg.ext_kb=1024;cfg.synchronous_video=true;
 if(posix_memalign((void**)&cfg.ram,4096,PC_RAM_BYTES(cfg.ext_kb)))return -1;cfg.page_table=calloc(X86_PAGES,sizeof(uintptr_t));
 if(!pc_init(&cfg)||!pc_floppy_insert(0,boot)){message("FreeDOS boot image missing or invalid: /DOS/FREEDOS.IMG");return -1;}
 int mouse_driver=pc_floppy_insert(1,"/System/DOS/DRIVERS.IMG");
 if(!mouse_driver)message("Optional DOS mouse driver missing: /System/DOS/DRIVERS.IMG");
 if(!pc_hd_insert(0,dos_mount_root)){message("Cannot mount the selected program folder.");return -1;}
 if(arg&&!dos_folder_name(disks[2].folder,strrchr(arg,'/')+1,program)){message("The program folder exceeds DOS limits, or its short name cannot be resolved.");return -1;}
 uint32_t identity=tab5_crc32(0,dos_mount_root,strlen(dos_mount_root));identity^=dos_folder_fingerprint(disks[2].folder);
 /* Include the complete immutable boot image in the snapshot identity. */
 int bh=base->open_read(boot);if(bh<0)return -1;uint8_t block[1024];int n;while((n=base->read(bh,block,sizeof(block)))>0)identity=tab5_crc32(identity,block,(size_t)n);base->close(bh);if(n<0)return -1;
 dos_overlay_init(identity);int restored=dos_overlay_load();if(restored<0)message("Saved DOS disk changes are invalid; starting from the unchanged base.");
 disks[0].readonly=false; /* C: remains read-only; only the floppy uses COW. */
 pixels=malloc(W*H*2);line=malloc(1024*2);
 if(canvas->canvas_begin(W|TAB5_CANVAS_MOUSE_EVENTS,H)){message("Update firmware and SD together for DOS mouse support.");return -1;}pc_reset();dos_audio_start();
 struct dos_startup startup={0};uint32_t rendered=0;struct dos_frame_pacer pacing={0};
 uint32_t stats_at=base->millis(),stats_frames=pc.frames,stats_presented=0,stats_cpu=0,stats_render=0,stats_audio=0;
 render();
 while(!quit&&!base->should_stop()){
  uint32_t start=base->millis();input();if(quit)break;
  int present=ui_dirty||start-rendered>=40;pc_video_capture_next(present);
  uint32_t cpu_at=base->millis();pc_run_frame();stats_cpu+=base->millis()-cpu_at;
  if(pc_halted()){message("The guest CPU halted. This program may require a 386 or newer PC.");quit=1;break;}
  if(!startup.step&&screen_contains("TAB5OS DOS READY")){dos_startup_begin(&startup);message("FreeDOS ready. C: contains the selected SD folder.");}
  int startup_action=dos_startup_poll(&startup,mouse_driver,base->language&&base->language[0]=='f',layout_pending>=0,program);
  if(startup_action==1)change_layout();
  else if(startup_action==-1)message("Cannot select drive C:. Automatic launch stopped; the DOS prompt remains available.");
  else if(startup_action==-2)message("DOS startup did not reach a command prompt. Continue manually at the DOS prompt.");
  if(layout_pending>=0&&!dos_keyboard_typing()){
   if(!layout_wait)layout_wait=pc.frames;
   if(keyboard_vector()!=layout_vector&&pc.frames-layout_wait>5){dos_french_keyboard=layout_pending;layout_pending=-1;message(dos_french_keyboard?"French AZERTY keyboard enabled.":"US keyboard enabled.");}
   else if(pc.frames-layout_wait>500){layout_pending=-1;message("Keyboard driver did not switch. Use the current layout at the DOS prompt.");}
  }
  uint32_t audio_at=base->millis();if(dos_audio_pump()<0)message("Audio unavailable: adjust volume to retry.");stats_audio+=base->millis()-audio_at;
  uint32_t now=base->millis();if(present){stats_presented+=(uint32_t)render();stats_render+=base->millis()-now;rendered=now;}
  if(now-stats_at>=2000){uint32_t elapsed=now-stats_at,frames=pc.frames-stats_frames;unsigned hz=(unsigned)pc_screen_hz();
   unsigned speed=hz?(unsigned)((uint64_t)frames*100000u/((uint64_t)elapsed*hz)):0,fps=stats_presented*1000u/elapsed;
   snprintf(performance,sizeof(performance),"PC %u%% | %u fps",speed,fps);ui_dirty=1;
   printf("DOS performance: guest=%u%%, display=%u fps, CPU=%u ms, video=%u ms, audio=%u ms / %u ms\n",speed,fps,stats_cpu,stats_render,stats_audio,elapsed);
   stats_at=now;stats_frames=pc.frames;stats_presented=stats_cpu=stats_render=stats_audio=0;}
  base->sleep_ms(dos_frame_delay(&pacing,start,base->millis(),pc_screen_hz()));
 }
 release_touch();pc_audio_enable(false);int saved=dos_overlay_save();if(saved)message("DOS changes could not be saved. The previous saved session is intact.");
 pc_floppy_eject(0);pc_floppy_eject(1);pc_hd_eject(0);return saved?-1:0;
}
