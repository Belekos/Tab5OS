/* Tab5OS x86: Tiny386 with a 32-bit CPU, BIOS and FreeDOS in an isolated application.
 * All hardware access goes through SDK callbacks. Guest files are read-only
 * bases; changed sectors use a bounded, explicitly saved copy-on-write layer. */
#include "../dos/platform.h"
#include "storage.h"
#include "machine.h"
#include "../common/runtime.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "app_mouse.h"
#include "presentation.h"

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
static char stats_details[3][48];
static int info_visible;
int dos_french_keyboard;

#include "startup.h"
extern const uint8_t pc_font_bios8[1024];
static void message(const char *s){snprintf(notice,sizeof(notice),"%s",s);ui_dirty=1;puts(s);}
/* DOS_UI_PRIMITIVES_BEGIN: production rendering also exercised on the host. */
static void rect(int x,int y,int w,int h,uint16_t color){for(int yy=y;yy<y+h&&yy<H;yy++)for(int xx=x;xx<x+w&&xx<W;xx++)if(xx>=0&&yy>=0)pixels[yy*W+xx]=color;}
static void text_scaled(int x,int y,const char *s,uint16_t color,int scale){for(;*s&&x<W;s++,x+=8*scale){unsigned c=(unsigned char)*s;if(c>=128)c='?';for(int row=0;row<8;row++)for(int col=0;col<8;col++)if(pc_font_bios8[c*8+row]&(128>>col))rect(x+col*scale,y+row*scale,scale,scale,color);}}
static void text(int x,int y,const char *s,uint16_t color){text_scaled(x,y,s,color,1);}
static int text_wrap(int x,int y,const char *s,int columns,int scale,uint16_t color){
 while(*s){char row[96];int n=0;while(s[n]&&s[n]!='\n'&&n<columns)n++;memcpy(row,s,(size_t)n);row[n]=0;text_scaled(x,y,row,color,scale);s+=n;if(*s=='\n')s++;y+=10*scale;}return y;
}
struct hit {int x,y,w,h;uint16_t code;char ch;int action;};
static struct hit hits[100];static int hit_count;
enum {ACT_NONE,ACT_SAVE,ACT_KEYS,ACT_EXIT,ACT_LAYOUT,ACT_SHIFT,ACT_CTRL,ACT_ALT,ACT_VOL_DOWN,ACT_VOL_UP,ACT_MUTE,ACT_INFO};
static void button(int x,int y,int w,int h,const char *label,int action,uint16_t code,char ch,int active){
 int length=(int)strlen(label),scale=h>=24&&length*16+8<=w?2:1;
 rect(x,y,w,h,active?0x126f:0x2945);rect(x,y,w,1,0x630c);text_scaled(x+(w-length*8*scale)/2,y+(h-8*scale)/2,label,0xffff,scale);
 if(hit_count<100)hits[hit_count++]=(struct hit){x,y,w,h,code,ch,action};
}
/* DOS_UI_PRIMITIVES_END */
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
 if(!before&&input_sources[index])machine_key(scan,true);else if(before&&!input_sources[index])machine_key(scan,false);
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
static void change_layout(void){
 if(layout_pending>=0||machine_typing())return;
 if(dos_french_keyboard)machine_type("keyb /u\n");else machine_type("keyb fr,437,a:\\keyboard.sys\n");
 layout_pending=!dos_french_keyboard;layout_wait=0;layout_vector=machine_keyboard_vector();message("Changing DOS keyboard layout...");
}
/* DOS_UI_ACTIONS_BEGIN */
static void action(int a){ui_dirty=1;switch(a){
 case ACT_SAVE:message(dos_overlay_save()?"Save failed; changes remain in this session.":"DOS changes saved.");break;
 case ACT_KEYS:dos_pointer_cancel();keyboard_visible=!keyboard_visible;hit_count=0;break;
 case ACT_INFO:dos_pointer_cancel();info_visible=!info_visible;hit_count=0;break;
 case ACT_EXIT:if(dos_overlay_save())message("Save failed. Session kept open; retry Save or force stop to discard.");else quit=1;break;
 case ACT_LAYOUT:change_layout();break;
 case ACT_VOL_DOWN:dos_audio_change(-10);break;case ACT_VOL_UP:dos_audio_change(10);break;case ACT_MUTE:dos_audio_mute();break;
 case ACT_SHIFT:shift=!shift;break;case ACT_CTRL:control=!control;input_key(29,control,4);break;case ACT_ALT:alt=!alt;input_key(56,alt,4);break;default:break;}}
static void release_touch(void){if(held_touch){input_key(held_touch,false,2);held_touch=0;}}
static int pointer_control(int x,int y){
 for(int i=0;i<hit_count;i++){struct hit *h=&hits[i];if(x<h->x||y<h->y||x>=h->x+h->w||y>=h->y+h->h)continue;
  if(h->action)action(h->action);else if(h->ch){char value[2]={shift?(char)toupper(h->ch):h->ch,0};machine_type(value);}
  else if(h->code){held_touch=h->code;input_key(h->code,true,2);}return 1;
 }return info_visible;
}
/* DOS_UI_ACTIONS_END */
static void input(void){
 static unsigned mouse_buttons;static int mouse_control;
 physical_keyboard();struct tab5_app_input_event e={0};e.size=sizeof(e);
 for(int n=0;n<64&&canvas->input_poll(&e)>0;n++){
  if(e.type==TAB5_INPUT_CLOSE){dos_pointer_cancel();action(ACT_EXIT);continue;}
  if(e.type==TAB5_INPUT_POINTER_POSITION){
   if(!(e.key&1u)&&mouse_control){release_touch();mouse_control=0;}
   if((e.key&1u)&&!(mouse_buttons&1u)){release_touch();mouse_control=pointer_control(e.x,e.y);}
   mouse_buttons=e.key;if(!info_visible)dos_pointer_mouse_position(mouse_control?-1:e.x,mouse_control?-1:e.y,mouse_control?0:e.key);continue;
  }
  if(e.type==TAB5_INPUT_POINTER_RELATIVE){if(!info_visible)dos_pointer_mouse_relative(e.x,e.y);continue;}
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
/* DOS_UI_RENDER_BEGIN */
static void toolbar(int y,int height){
 char volume[12];snprintf(volume,sizeof(volume),dos_audio_failed()?"ERR":"%d%%",dos_audio_volume());
 button(4,y,92,height,"Save",ACT_SAVE,0,0,dos_overlay_dirty());button(104,y,92,height,"Keys",ACT_KEYS,0,0,keyboard_visible);
 button(204,y,104,height,dos_french_keyboard?"FR":"US",ACT_LAYOUT,0,0,0);
 button(316,y,56,height,"-",ACT_VOL_DOWN,0,0,0);button(380,y,72,height,volume,ACT_MUTE,0,0,!dos_audio_volume());button(460,y,56,height,"+",ACT_VOL_UP,0,0,0);
 button(524,y,64,height,"Esc",0,1,0,0);button(596,y,64,height,"F5",0,63,0,0);button(668,y,128,height,"Info",ACT_INFO,0,0,info_visible);
}
static int render(void){
 static uint32_t last_signature;static int last_w,last_h,last_keyboard=-1,last_dirty=-1,last_text=-1,last_info=-1;
 int w,h;uint32_t signature=machine_screen_signature();machine_screen_size(&w,&h);
 int text_mode=machine_text_mode();
 int layout=last_w!=w||last_h!=h||last_keyboard!=keyboard_visible||last_text!=text_mode||last_info!=info_visible;
 int changed=layout||last_signature!=signature;
 int dirty=dos_overlay_dirty();
 if(!changed&&!ui_dirty&&dirty==last_dirty)return 0;
 if(layout)memset(pixels,0,W*H*2);
 else if(keyboard_visible)rect(0,252,W,H-252,0);
 else if(text_mode)rect(0,400,W,H-400,0);
 else {rect(624,0,W-624,H,0);rect(0,468,W,12,0);}
 struct dos_viewport view=dos_display_viewport(w,h,text_mode,keyboard_visible);
 dos_pointer_viewport(view.x,view.y,view.w,view.h,w,h);
 if(view.w&&changed)dos_present_screen(pixels+view.y*W+view.x,W,w,h,view.w,view.h,line,machine_render_line);
 hit_count=0;
 char volume[12];snprintf(volume,sizeof(volume),dos_audio_failed()?"ERR":"%d%%",dos_audio_volume());
 if(keyboard_visible)toolbar(256,30);
 else if(text_mode){toolbar(408,36);text(8,450,performance,0x7bef);}
 else {
  button(632,4,160,38,"Keyboard",ACT_KEYS,0,0,keyboard_visible);
  button(632,48,76,38,"Esc",0,1,0,0);button(716,48,76,38,"F5",0,63,0,0);
  button(632,92,160,38,"Save",ACT_SAVE,0,0,dirty);button(632,136,160,38,dos_french_keyboard?"FR keys":"US keys",ACT_LAYOUT,0,0,0);
  text_scaled(632,190,"Volume",0xbdf7,2);
  button(632,214,64,36,volume,ACT_MUTE,0,0,!dos_audio_volume());button(704,214,40,36,"-",ACT_VOL_DOWN,0,0,0);button(752,214,40,36,"+",ACT_VOL_UP,0,0,0);
  button(632,266,160,38,"Info",ACT_INFO,0,0,info_visible);
  char speed[40];snprintf(speed,sizeof(speed),"%s",performance);char *fps=strchr(speed,'|');if(fps){*fps++=0;while(*fps==' ')fps++;}
  text_scaled(632,330,speed,0x7bef,2);if(fps)text_scaled(632,354,fps,0x7bef,2);text(632,396,"4:3 picture",0xbdf7);
 }
 if(keyboard_visible){
  button(4,294,48,28,"Esc",0,1,0,0);for(int i=0;i<12;i++){char f[5];snprintf(f,sizeof(f),"F%d",i+1);button(i<9?56+i*42:434+(i-9)*58,294,i<9?40:56,28,f,0,(uint16_t)(i<10?59+i:87+i-10),0,0);}button(612,294,64,28,"Tab",0,15,0,0);button(680,294,112,28,"Back",0,14,0,0);
  const char *rows[]={"1234567890-=/\\",dos_french_keyboard?"azertyuiop[]":"qwertyuiop[]",dos_french_keyboard?"qsdfghjklm;:'":"asdfghjkl;:'",dos_french_keyboard?"wxcvbn,.!?":"zxcvbnm,.!?"};
  for(int y=0;y<4;y++)for(int x=0;rows[y][x];x++){char label[2]={shift?(char)toupper(rows[y][x]):rows[y][x],0};button(4+x*43,326+y*28,41,26,label,0,0,rows[y][x],0);}
  button(610,326,88,54,"Enter",0,28,0,0);button(704,326,90,26,"Del",0,0xe053,0,0);button(704,354,90,26,"^",0,0xe048,0,0);
  button(610,382,58,26,"<",0,0xe04b,0,0);button(670,382,58,26,"v",0,0xe050,0,0);button(730,382,64,26,">",0,0xe04d,0,0);
  button(4,440,88,28,"Shift",ACT_SHIFT,0,0,shift);button(100,440,72,28,"Ctrl",ACT_CTRL,0,0,control);button(180,440,72,28,"Alt",ACT_ALT,0,0,alt);button(260,440,280,28,"Space",0,0,' ',0);button(548,440,110,28,"Home",0,0xe047,0,0);button(666,440,128,28,"End",0,0xe04f,0,0);
 }
 text(8,470,notice,0xbdf7);
 if(info_visible){
  rect(40,32,720,400,0x1082);rect(40,32,720,2,0x630c);hit_count=0;
  text_scaled(64,50,"DOS x86 performance",0xffff,2);button(624,44,112,36,"Close",ACT_INFO,0,0,0);
  text_scaled(64,102,performance,0xffff,2);
  int y=146;for(int i=0;i<3;i++)y=text_wrap(64,y,stats_details[i],41,2,0xbdf7)+6;
  text(64,292,"MIPS = millions of emulated instructions per second.",0xbdf7);
  text(64,310,"Times are measured over the period, not P4 core load.",0xbdf7);
  text(64,328,"i686-compatible core, x87, 8 MiB RAM, VGA/VBE, SB16.",0xbdf7);
  text_wrap(64,356,notice,82,1,0xffff);
 }
 tab5_api->video_present(pixels,W*H);
 last_w=w;last_h=h;last_keyboard=keyboard_visible;last_text=text_mode;last_info=info_visible;last_signature=signature;last_dirty=dirty;ui_dirty=0;return 1;
}
/* DOS_UI_RENDER_END */
static int executable(const char *path){
 const char *ext=strrchr(path,'.');if(!ext)return 0;
 int com=!strncasecmp(ext,".com",5),exe=!strncasecmp(ext,".exe",5);if(!com&&!exe)return 0;
 int h=base->open_read(path);if(h<0)return 0;uint32_t bytes=tab5_api->file_size(h);uint8_t header[64]={0};int n=base->read(h,header,sizeof(header));int ok=com?bytes>0&&bytes<=65280:n>=28&&header[0]=='M'&&header[1]=='Z';
 if(ok&&exe&&n>=64){uint32_t off=(uint32_t)header[60]|(uint32_t)header[61]<<8|(uint32_t)header[62]<<16|(uint32_t)header[63]<<24;if(off>=28&&off<=bytes-4){uint8_t sig[4];if(!base->seek(h,off)&&base->read(h,sig,4)==4&&((sig[0]=='P'&&sig[1]=='E')||(sig[0]=='N'&&sig[1]=='E')))ok=0;}}
 base->close(h);return ok;
}
int tab5_app_main(int argc,char **argv){
 dos_api=tab5_app_api_v7();if(!dos_api)return -1;canvas=&dos_api->base;
 const struct tab5_app_api2 *video=&canvas->base.base.base.base;
 if(video->base.version<7||video->base.size<sizeof(*dos_api)||!dos_api->keyboard_state||!dos_api->directory_open||!dos_api->directory_next||!canvas->canvas_begin||!canvas->input_poll)return -1;
 tab5_runtime_init(video);base=&tab5_api->base;
 tab5_runtime_capacity(16u*1024u*1024u);
 if(__builtin_setjmp(tab5_escape)){
  puts("DOS x86 stopped after an allocation or runtime error.");
  x86_storage_close();return -1;
 }
 const char *arg=argc>1?argv[1]:NULL;
 snprintf(dos_mount_root,sizeof(dos_mount_root),"/DOS");
 if(arg&&*arg){
  if(strlen(arg)>=256||arg[0]!='/'||!strcmp(arg,"/USB")||!strncmp(arg,"/USB/",5)){message("DOS requires a file on the SD card.");return -1;}
  if(!executable(arg)){message("Choose a DOS .COM or MZ .EXE, including DOS extenders; Windows executables are unsupported.");return -1;}
  strcpy(dos_mount_root,arg);char *slash=strrchr(dos_mount_root,'/');if(slash==dos_mount_root)slash[1]=0;else *slash=0;
 }
 message("Booting FreeDOS on the x86 32-bit engine with x87, VGA/VBE and SB16.");
 if(x86_storage_open(dos_mount_root,arg,program)){
  message("Cannot open the DOS boot image or selected folder.");x86_storage_close();return -1;
 }
 dos_overlay_init(x86_storage_identity());
 if(dos_overlay_load()<0)message("Saved DOS disk changes are invalid; starting from the unchanged base.");
 int mouse_driver=x86_storage_has_mouse_driver();
 if(!mouse_driver)message("Optional DOS mouse driver missing: /System/DOS/DRIVERS.IMG");
 if(machine_init(mouse_driver)){
  message("Cannot initialize the x86 machine.");x86_storage_close();return -1;
 }
 pixels=malloc(W*H*2);line=malloc(MACHINE_FB_WIDTH*2);
 if(canvas->canvas_begin(W|TAB5_CANVAS_MOUSE_EVENTS,H)){
  message("Update firmware and SD together for DOS mouse support.");x86_storage_close();return -1;
 }
 dos_audio_start();
 struct dos_startup startup={0};uint32_t rendered=base->millis();
 uint32_t stats_at=base->millis(),stats_insns=machine_instructions();
 uint32_t stats_presented=0,stats_cpu=0,stats_render=0,stats_audio=0;
 snprintf(performance,sizeof(performance),"0.00 MIPS | 0 fps");
 machine_video(1);render();
 while(!quit&&!base->should_stop()){
  uint32_t start=base->millis();input();if(quit)break;
  /* Short instruction batches keep mouse, audio and stop requests responsive. */
  unsigned batches=0;uint32_t cpu_at=base->millis();
  do{machine_step();++batches;}while(batches<8u&&base->millis()-cpu_at<4u&&!base->should_stop());
  stats_cpu+=base->millis()-cpu_at;
  if(machine_stopped()){message("The guest requested shutdown.");quit=1;break;}
  if(!startup.step&&machine_screen_contains("TAB5OS DOS READY")){
   dos_startup_begin(&startup);message("FreeDOS ready. C: contains the selected SD folder.");
  }
  int startup_action=dos_startup_poll(&startup,mouse_driver,base->language&&base->language[0]=='f',layout_pending>=0,program);
  if(startup_action==1)change_layout();
  else if(startup_action==-1)message("Cannot select drive C:. Automatic launch stopped; the DOS prompt remains available.");
  else if(startup_action==-2)message("DOS startup did not reach a command prompt. Continue manually at the DOS prompt.");
  if(layout_pending>=0&&!machine_typing()){
   uint32_t now=base->millis();if(!layout_wait)layout_wait=now;
   if(machine_keyboard_vector()!=layout_vector&&now-layout_wait>100u){dos_french_keyboard=layout_pending;layout_pending=-1;message(dos_french_keyboard?"French AZERTY keyboard enabled.":"US keyboard enabled.");}
   else if(now-layout_wait>10000u){layout_pending=-1;message("Keyboard driver did not switch. Use the current layout at the DOS prompt.");}
  }
  uint32_t audio_at=base->millis();if(dos_audio_pump()<0)message("Audio unavailable: adjust volume to retry.");stats_audio+=base->millis()-audio_at;
  uint32_t now=base->millis();int present=ui_dirty||now-rendered>=40u;
  machine_video(present);
  if(present){stats_presented+=(uint32_t)render();rendered=base->millis();}
  stats_render+=base->millis()-now;
  now=base->millis();
  if(now-stats_at>=2000u){
   uint32_t elapsed=now-stats_at,instructions=machine_instructions();
   unsigned hundredths=(unsigned)((uint64_t)(instructions-stats_insns)*100u/((uint64_t)elapsed*1000u));
   unsigned fps=stats_presented*1000u/elapsed;
   snprintf(performance,sizeof(performance),"%u.%02u MIPS | %u fps",hundredths/100u,hundredths%100u,fps);ui_dirty=1;
   snprintf(stats_details[0],sizeof(stats_details[0]),"CPU %u ms | Audio %u ms",stats_cpu,stats_audio);
   snprintf(stats_details[1],sizeof(stats_details[1]),"Video %u ms | Period %u ms",stats_render,elapsed);
   size_t arena,available,largest;tab5_runtime_memory(&arena,&available,&largest);
   snprintf(stats_details[2],sizeof(stats_details[2]),"Heap free %uK | block %uK",(unsigned)(available/1024),(unsigned)(largest/1024));
   printf("DOS x86 performance: %u.%02u MIPS, display=%u fps, CPU=%u ms, video=%u ms, audio=%u ms / %u ms\n",hundredths/100u,hundredths%100u,fps,stats_cpu,stats_render,stats_audio,elapsed);
   printf("DOS heap: budget=%u KiB, free=%u KiB, largest=%u KiB\n",(unsigned)(arena/1024),(unsigned)(available/1024),(unsigned)(largest/1024));
   stats_at=now;stats_insns=instructions;stats_presented=stats_cpu=stats_render=stats_audio=0;
  }
  (void)start;base->sleep_ms(0);
 }
 release_touch();dos_pointer_cancel();machine_cancel_input();
 int saved=dos_overlay_save();if(saved)message("DOS changes could not be saved. The previous saved session is intact.");
 x86_storage_close();return saved?-1:0;
}
