#include <lvgl.h>
#include <Arduino.h>
#include <algorithm>
#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_heap_caps.h>
#include <Preferences.h>
#include <Tab5Audio.h>
#include <Tab5AudioDriver.h>
#include <M5Unified.h>
uint32_t testMillis=0;
std::vector<unsigned char> nvsBytes;
bool nvsOpenOk=true,nvsWriteOk=true,taskCreateOk=true,configureOk=true;
unsigned nvsWrites=0,leaseAcquired=0,leaseReleased=0,claims=0,releases=0,stops=0,pcmWrites=0;
void(*pendingTask)(void*)=nullptr;void* pendingArgument=nullptr;void(*onPcm)()=nullptr;
AudioStub Tab5Audio;DriverStub Tab5AudioDevice;M5Stub M5;
size_t heap_caps_get_free_size(unsigned){return 32*1024*1024;}
void*heap_caps_malloc(size_t n,unsigned){return malloc(n);}void heap_caps_free(void*p){free(p);}
#include "Tab5Alarm.cpp"
#include "Tab5Icons.h"
static unsigned checks=0;
static void check(bool result,const char* message){++checks;if(!result){fprintf(stderr,"FAIL %s\n",message);exit(1);}}
static lv_display_t* display;static std::vector<uint8_t> pixels(1280u*720u*3u);
static void pump(){testMillis+=40;lv_tick_inc(40);lv_timer_handler();lv_obj_update_layout(lv_screen_active());lv_obj_update_layout(lv_layer_top());}
static void runTask(){check(pendingTask!=nullptr,"scheduled worker exists");auto f=pendingTask;auto a=pendingArgument;pendingTask=nullptr;onPcm=[](){if(pcmWrites%4==0)Tab5Alarm::soundWanted=false;};f(a);onPcm=nullptr;}
static uint32_t palette[]={0xf4f4f0,0xffffff,0xe8e8e4,0x777777,0x161616,0x666666,0x2244cc,0x161616,0xaadd99};
static uint32_t alarmColor(unsigned i){static unsigned roles[]={0,2,4,5,3,6};return palette[i<6?roles[i]:4];}
static const char* english(const char* s){
 struct Pair{const char*a,*b;};static Pair p[]={{"Réveil","Alarm clock"},{"Vérifiez la date, l’heure et le fuseau horaire dans les réglages.","Check date, time and time zone in Settings."},{"Activer le réveil","Enable alarm"},{"Sonnerie du réveil","Alarm tone"},{"Classique\nCarillon\nPulsations\nDoux","Classic\nChime\nPulses\nSoft"},{"Volume du réveil","Alarm volume"},{"Enregistrer","Save"},{"Essayer la sonnerie","Test sound"},{"Veille jusqu’au réveil","Sleep until alarm"},{"Fermer","Close"},{"Réveil désactivé","Alarm disabled"},{"La veille prolongée redémarre la tablette avant la sonnerie. Gardez une batterie chargée ou une alimentation branchée. La charge de la batterie est interrompue pendant cette veille.","Deep sleep restarts the tablet before the alarm. Keep a charged battery or external power connected. Battery charging stops during this sleep."},{"Une fois\nTous les jours","Once\nEvery day"}};
 for(auto x:p)if(!strcmp(s,x.a))return x.b;return s;
}
static bool bg(lv_obj_t*o,lv_part_t p,uint32_t expected){return lv_color_to_u32(lv_obj_get_style_bg_color(o,p))==lv_color_to_u32(lv_color_hex(expected));}
static void styleTests(){
 auto* host=lv_obj_create(lv_screen_active());auto*b=lv_button_create(host);auto*semantic=lv_button_create(host);auto*cb=lv_checkbox_create(host);auto*dd=lv_dropdown_create(host);auto*sw=lv_switch_create(host);
 Tab5UiStyle::configure(palette,9,0x123456,0x456789,0xabcdef,0xfedcba);Tab5UiStyle::button(b);lv_obj_set_style_bg_color(semantic,lv_color_hex(0xabcdef),0);Tab5UiStyle::selection(semantic);Tab5UiStyle::tree(host);lv_dropdown_set_options(dd,"One\nTwo\nThree");
 for(auto*o:{b,semantic,cb,dd,sw})lv_obj_set_style_transition(o,nullptr,LV_STATE_ANY);
 check(bg(semantic,LV_PART_MAIN,0xabcdef),"semantic normal button fill retained");
 for(auto state:{LV_STATE_CHECKED,LV_STATE_PRESSED,lv_state_t(LV_STATE_CHECKED|LV_STATE_PRESSED),lv_state_t(LV_STATE_CHECKED|LV_STATE_FOCUSED|LV_STATE_FOCUS_KEY),lv_state_t(LV_STATE_CHECKED|LV_STATE_DISABLED)}){
  for(auto*o:{b,semantic,cb}){lv_obj_remove_state(o,LV_STATE_ANY);lv_obj_add_state(o,state);check(bg(o,o==cb?LV_PART_INDICATOR:LV_PART_MAIN,palette[8]),"selected button/check follows selection palette");check(lv_obj_get_style_color_filter_opa(o,o==cb?LV_PART_INDICATOR:LV_PART_MAIN)==0,"native theme filter does not tint selection");}
 }
 lv_dropdown_open(dd);pump();auto*list=lv_dropdown_get_list(dd);check(list!=nullptr,"dropdown list exists");lv_obj_set_style_transition(list,nullptr,LV_STATE_ANY);
 for(auto state:{LV_STATE_DEFAULT,LV_STATE_CHECKED,LV_STATE_PRESSED,lv_state_t(LV_STATE_CHECKED|LV_STATE_PRESSED),lv_state_t(LV_STATE_CHECKED|LV_STATE_FOCUS_KEY)}){lv_obj_remove_state(list,LV_STATE_ANY);lv_obj_add_state(list,state);check(bg(list,LV_PART_SELECTED,palette[8]),"dropdown selected part uses configured selection");}
 check(bg(sw,LV_PART_MAIN,0x123456)&&bg(sw,LV_PART_KNOB,0xabcdef),"custom switch off colors preserved");lv_obj_add_state(sw,LV_STATE_CHECKED);check(bg(sw,LV_PART_INDICATOR,0x456789)&&bg(sw,LV_PART_KNOB,0xfedcba),"custom switch on colors preserved");
 palette[8]=0xd4aacc;Tab5UiStyle::configure(palette,11);Tab5Focus.theme(lv_color_hex(palette[8]),lv_color_hex(palette[4]),lv_color_hex(palette[1]));Tab5UiStyle::tree(host);check(bg(b,LV_PART_MAIN,palette[8]),"theme refresh updates marked selected buttons");lv_dropdown_close(dd);lv_obj_delete(host);
}
static void toneTests(){
 std::array<uint64_t,4>hashes{};for(uint8_t t=0;t<4;++t){uint64_t h=1469598103934665603ull;uint32_t n=0;int16_t block[257];for(unsigned b=0;b<200;++b){Tab5AlarmTone::render(t,n,block,257);for(unsigned j=0;j<257;++j){const unsigned i=b*257+j;check(block[j]==Tab5AlarmTone::sample(t,i),"tone independent of PCM chunk boundaries");check(abs(int(block[j]))<=11000,"bounded PCM amplitude");h=(h^uint16_t(block[j]))*1099511628211ull;}}hashes[t]=h;check(Tab5AlarmTone::sample(t,0)==0&&Tab5AlarmTone::sample(t,47999)==0,"tone cycle silence endpoints");}
 for(unsigned i=0;i<4;++i)for(unsigned j=i+1;j<4;++j)check(hashes[i]!=hashes[j],"four distinct tones");check(Tab5AlarmTone::sample(250,245)==Tab5AlarmTone::sample(0,245),"legacy unknown tone falls back safely");
}
static void alarmTests(const char*shot){using namespace Tab5Alarm;
 Tab5AlarmPolicy::State legacy;legacy.hour=9;legacy.minute=37;legacy.volume=191;legacy.reserved[0]=250;nvsBytes.assign((uint8_t*)&legacy,(uint8_t*)&legacy+sizeof legacy);
 Hooks h;h.translate=english;h.font=[](){return &lv_font_montserrat_22;};h.color=alarmColor;begin(h);
 check(state.hour==9&&state.minute==37&&state.volume==191&&state.reserved[0]==0&&nvsWrites==0,"legacy NVS retained with safe tone fallback without forced write");
 open();pump();check(visible()&&tone&&lv_dropdown_get_option_count(tone)==4,"form exposes four tones");
 auto*originalPanel=panel;lv_dropdown_set_selected(tone,2);lv_slider_set_value(volume,77,LV_ANIM_OFF);lv_dropdown_set_selected(hours,8);const unsigned writes=nvsWrites;const uint8_t oldVolume=Tab5AudioDevice.volume();
 lv_obj_send_event(previewButton,LV_EVENT_CLICKED,nullptr);check(preview&&!alert&&soundRunning&&panel==originalPanel,"preview leaves form open");check(nvsWrites==writes&&state.hour==9&&soundVolume==77&&soundTone==2,"preview uses unsaved controls without persisting");
 runTask();check(!soundRunning&&Tab5AudioDevice.volume()==oldVolume&&claims==releases&&leaseAcquired==leaseReleased,"worker returns audio owner lease and volume");
 lv_obj_send_event(previewButton,LV_EVENT_CLICKED,nullptr);check(!preview,"test button stops preview");
 taskCreateOk=false;togglePreview();check(!preview&&!soundRunning&&soundFailed&&nvsWrites==writes,"task allocation failure does not save or retain owner");taskCreateOk=true;
 togglePreview();check(preview,"preview can restart");testMillis+=10001;service();check(!preview&&!soundWanted,"preview times out at ten seconds");runTask();
 togglePreview();writing::KeyEvent escape{};escape.key=writing::Key::Escape;key(escape);check(!visible()&&!preview&&!soundWanted,"Escape closes and stops preview");runTask();
 open();pump();lv_dropdown_set_selected(tone,3);lv_slider_set_value(volume,82,LV_ANIM_OFF);lv_dropdown_set_selected(hours,6);commit();check(state.reserved[0]==3&&state.hour==6&&state.volume==82&&nvsWrites==writes+1,"Save persists selected tone with existing layout");check(nvsBytes.size()==sizeof(Tab5AlarmPolicy::State),"persistent state size unchanged");
 auto saved=state;nvsWriteOk=false;lv_dropdown_set_selected(tone,1);commit();check(!memcmp(&state,&saved,sizeof state),"failed NVS write restores settings");nvsWriteOk=true;persistenceOk=true;notice=nullptr;
 togglePreview();state.enabled=1;state.daily=0;state.next=time(nullptr);lastPoll=0;testMillis+=300;service();check(ringPending&&!preview&&!soundWanted,"real alarm queues while preview worker retires");runTask();service();check(alert&&!ringPending&&soundRunning,"real alarm starts after prior owner released");dismiss();runTask();check(!alert&&!visible()&&claims==releases&&leaseAcquired==leaseReleased,"alarm dismiss releases resources");
 state=Tab5AlarmPolicy::State{};open();pump();Tab5Focus.focusObject(previewButton);pump();
 auto* icon=lv_obj_create(lv_screen_active());lv_obj_set_size(icon,60,50);tab5AlarmIcon(icon,lv_color_black(),lv_color_hex(palette[8]));check(lv_obj_get_child_count(icon)==1,"alarm icon owns vector primitives");lv_obj_delete(icon);pump();
 lv_obj_invalidate(lv_layer_top());lv_refr_now(display);FILE*f=fopen(shot,"wb");check(f!=nullptr,"capture opens");fprintf(f,"P6\n1280 720\n255\n");for(size_t i=0;i<pixels.size();i+=3){uint8_t rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,f);}fclose(f);close();
 for(unsigned i=0;i<100;++i){open();pump();close();}check(!visible()&&!soundRunning,"100 form cycles close cleanly");
}
int main(int argc,char**argv){check(argc==2,"capture argument");lv_init();display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);static std::vector<uint8_t>draw(1280*40*3);lv_display_set_buffers(display,draw.data(),nullptr,draw.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*a,uint8_t*data){for(int y=a->y1;y<=a->y2;++y)memcpy(&pixels[(size_t(y)*1280+a->x1)*3],data+size_t(y-a->y1)*(a->x2-a->x1+1)*3,size_t(a->x2-a->x1+1)*3);lv_display_flush_ready(d);});Tab5Focus.begin(lv_screen_active(),&lv_font_montserrat_22);Tab5UiStyle::configure(palette,9);Tab5Focus.theme(lv_color_hex(palette[8]),lv_color_hex(palette[4]),lv_color_hex(palette[1]));toneTests();styleTests();alarmTests(argv[1]);Tab5Focus.clear();lv_deinit();printf("{\"passed\":true,\"checks\":%u,\"formCycles\":100,\"physicalDeviceTested\":false}\n",checks);}
