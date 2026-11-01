#include "Tab5SdrUi.h"
#include "Tab5Sdr.h"
#include "Tab5UiStyle.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include <new>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
static unsigned checks=0;static uint32_t clockMs=1000;
static void check(bool ok,const char* msg){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",msg);exit(1);}}
uint32_t millis(){return clockMs;}
class String:public std::string {public:using std::string::string;String(const std::string& s):std::string(s){};};
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;static std::unordered_map<void*,size_t> allocations;static int failAfter=-1;
void* heap_caps_malloc(size_t n,unsigned){if(failAfter==0)return nullptr;if(failAfter>0)--failAfter;void* p=malloc(n);if(p)allocations[p]=n;return p;}void heap_caps_free(void* p){if(p){check(allocations.erase(p)==1,"owned UI allocation freed once");free(p);}}
static std::unordered_map<std::string,std::string> disk;static bool failSave=false,storageReady=true;static unsigned writes=0;
struct Tab5StorageUse {explicit Tab5StorageUse(bool=true){}explicit operator bool()const{return storageReady;}};
struct File {std::string bytes;bool exists=false;explicit operator bool()const{return exists;}size_t size()const{return bytes.size();}size_t read(uint8_t* p,size_t n){n=std::min(n,bytes.size());memcpy(p,bytes.data(),n);return n;}void close(){}};
struct FS {bool exists(const char* path){return disk.count(path)!=0;}bool mkdir(const char* path){disk[path]="";return true;}File open(const char* path){auto it=disk.find(path);return it==disk.end()?File{}:File{it->second,true};}} tab5FS;
bool tab5AtomicRecover(const char*){return true;}bool tab5AtomicWrite(const char* path,const void* data,size_t size){++writes;if(failSave)return false;disk[path]=std::string(static_cast<const char*>(data),size);return true;}
struct AudioDevice {unsigned volume(){return 128;}} Tab5AudioDevice;
namespace Tab5Sdr {static bool running=false,scanningState=false,pausedState=false;static Settings received;static Scan requestedScan;static unsigned configurations=0;static Frame output;static bool ready=false;
bool configure(const Settings& s){received=s;++configurations;running=true;scanningState=false;return true;}bool scan(const Scan& s,const Settings& cfg){if(!valid(s))return false;requestedScan=s;received=cfg;received.hz=s.first;running=scanningState=true;return true;}void stop(){running=false;scanningState=false;}bool busy(){return running;}const char* status(){return "Prêt";}bool scanning(){return scanningState;}bool scanPaused(){return pausedState;}void pauseScan(bool p){pausedState=p;}Settings current(){return received;}bool spectrum(Frame& frame){if(!ready)return false;frame=output;ready=false;return true;}bool takeDetection(Detection&){return false;}bool detectionOverflow(){return false;}void volume(unsigned){}
}
/* PRODUCTION */
static lv_obj_t* keyboardTarget=nullptr;
static void keyboard(bool shown,lv_obj_t* target){keyboardTarget=shown?target:nullptr;}
static lv_obj_t* findButton(lv_obj_t* root,const char* caption){if(lv_obj_check_type(root,&lv_button_class)&&lv_obj_get_child_count(root)){auto* l=lv_obj_get_child(root,0);if(lv_obj_check_type(l,&lv_label_class)&&!strcmp(lv_label_get_text(l),caption))return root;}for(unsigned i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=findButton(lv_obj_get_child(root,i),caption))return found;return nullptr;}
static void click(const char* caption){auto* button=findButton(Tab5SdrUi::ui->pane,caption);if(!button)fprintf(stderr,"missing button: %s\n",caption);check(button!=nullptr,"requested action button exists");lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);}
static uint8_t pixels[1280*720*3];
static void snapshot(const char* name){lv_refr_now(lv_display_get_default());FILE* file=fopen(name,"wb");check(file!=nullptr,"snapshot output");fprintf(file,"P6\n1280 720\n255\n");for(unsigned i=0;i<1280*720;++i){fputc(pixels[i*3+2],file);fputc(pixels[i*3+1],file);fputc(pixels[i*3],file);}fclose(file);}
int main(){lv_init();auto* display=lv_display_create(1280,720);lv_display_set_default(display);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});uint32_t palette[]={0x101418,0x202428,0xe8e8e4,0x777777,0xffffff,0x555555,0x3288cf,0x15191d,0x334455};Tab5UiStyle::configure(palette,24);
 auto* parent=lv_obj_create(lv_screen_active());lv_obj_set_size(parent,1200,680);lv_obj_set_style_pad_all(parent,8,0);Tab5Focus.begin(lv_screen_active(),LV_FONT_DEFAULT);
 Tab5ToolsHooks hooks;hooks.keyboard=keyboard;hooks.translate=[](const char* text){return text;};hooks.font=[](){return &lv_font_montserrat_24;};hooks.color=[](unsigned n)->uint32_t{return n==2?0xffffff:n==4?0x3288cf:0x202428;};
 check(Tab5SdrUi::open(parent,hooks),"SDR opens");auto* u=Tab5SdrUi::ui;lv_obj_update_layout(parent);Tab5SdrUi::service();lv_obj_update_layout(parent);
 snapshot("sdr-spectrum.ppm");lv_tabview_set_active(u->tabs,1,LV_ANIM_OFF);snapshot("sdr-scan.ppm");lv_tabview_set_active(u->tabs,0,LV_ANIM_OFF);check(lv_obj_get_width(u->chart)>500&&lv_obj_get_height(u->chart)>=140,"spectrum usable area");check(lv_obj_get_style_radius(u->chart,LV_PART_MAIN)==0,"spectrum independent of 24px theme radius");Tab5UiStyle::tree(u->pane);check(lv_obj_get_style_radius(u->chart,LV_PART_MAIN)==0,"theme repaint keeps rectangular spectrum");
 auto* plus=findButton(u->pane,"+");auto* minus=findButton(u->pane,"-");lv_area_t plusArea,minusArea,fieldArea;lv_obj_get_coords(plus,&plusArea);lv_obj_get_coords(minus,&minusArea);lv_obj_get_coords(u->frequency,&fieldArea);
 check(plusArea.y1==minusArea.y1&&plusArea.y2==fieldArea.y2&&minusArea.y2==fieldArea.y2,"plus and minus align to input baseline");
 for(auto* b:{plus,minus,u->receive,lv_obj_get_child(lv_tabview_get_tab_bar(u->tabs),0)}){lv_obj_add_state(b,LV_STATE_PRESSED);lv_tick_inc(300);lv_timer_handler();check(lv_color_eq(lv_obj_get_style_bg_color(b,LV_PART_MAIN),lv_color_hex(palette[8])),"pressed button uses selected theme color");lv_obj_remove_state(b,LV_STATE_PRESSED);}
 // Move focus to a keyboard-like button while editing every position. The
 // old shared key path interpreted arrows as navigation in this situation.
 for(auto* field:{u->frequency,u->step,u->offset,u->ymin,u->ymax,u->first,u->last,u->dwell,u->threshold,u->ppm}){
  std::string original=lv_textarea_get_text(field);bool scanPage=field==u->first||field==u->last||field==u->dwell||field==u->threshold;lv_tabview_set_active(u->tabs,scanPage?1:0,LV_ANIM_OFF);lv_obj_update_layout(parent);
  lv_textarea_set_text(field,"123456.789");lv_obj_send_event(field,LV_EVENT_CLICKED,nullptr);Tab5Focus.focusObject(plus);
  for(int index=9;index>=0;--index){lv_textarea_set_cursor_pos(field,index+1);Tab5SdrUi::touchKey(writing::Key::Left);check(lv_textarea_get_cursor_pos(field)==unsigned(index),"touch arrow edits active field despite moved focus");Tab5SdrUi::touchKey(writing::Key::Delete);Tab5SdrUi::insert("0");check(lv_textarea_get_text(field)[index]=='0',"every input character replaceable");}
  Tab5SdrUi::touchKey(writing::Key::Home);check(lv_textarea_get_cursor_pos(field)==0,"input home reachable");Tab5SdrUi::touchKey(writing::Key::End);check(lv_textarea_get_cursor_pos(field)==10,"input end reachable");lv_textarea_set_text(field,original.c_str());
 }
 lv_tabview_set_active(u->tabs,0,LV_ANIM_OFF);lv_textarea_set_text(u->ppm,"0");lv_textarea_set_text(u->ymin,"-100");lv_textarea_set_text(u->ymax,"0");
 lv_obj_send_event(u->frequency,LV_EVENT_CLICKED,nullptr);check(keyboardTarget==u->frequency&&Tab5SdrUi::isInput(keyboardTarget),"touch frequency routes numeric field to shared keyboard");check(!strcmp(lv_textarea_get_accepted_chars(keyboardTarget),"0123456789."),"numeric keypad decimal contract");lv_textarea_set_text(u->frequency,"100");Tab5SdrUi::insert(".5");check(!strcmp(lv_textarea_get_text(u->frequency),"100.5"),"tactile decimal insertion");Tab5SdrUi::touchKey(writing::Key::Backspace);check(!strcmp(lv_textarea_get_text(u->frequency),"100."),"tactile backspace edits");Tab5SdrUi::insert("5");Tab5SdrUi::touchKey(writing::Key::Enter);check(Tab5Sdr::received.hz==100500000&&!keyboardTarget,"tactile Enter applies and closes keyboard");
 lv_dropdown_set_selected(u->unit,0);lv_obj_send_event(u->unit,LV_EVENT_VALUE_CHANGED,nullptr);check(!strcmp(lv_textarea_get_text(u->frequency),"100500.000"),"MHz to kHz preserves frequency");lv_textarea_set_text(u->frequency,"531");click("Recevoir");check(Tab5Sdr::received.hz==531000,"AM-band entry not rejected by UI");lv_textarea_set_text(u->step,"9");click("+");check(Tab5Sdr::received.hz==540000,"step applies live without stop");
 lv_dropdown_set_selected(u->mode,2);lv_obj_send_event(u->mode,LV_EVENT_VALUE_CHANGED,nullptr);check(Tab5Sdr::received.mode==2,"live AM mode applied");
 lv_textarea_set_text(u->ppm,"-18");lv_dropdown_set_selected(u->deemphasis,1);lv_obj_send_event(u->deemphasis,LV_EVENT_VALUE_CHANGED,nullptr);check(Tab5Sdr::received.ppm==-18&&Tab5Sdr::received.deemphasis==75,"ppm and de-emphasis applied live");lv_textarea_set_text(u->ppm,"999");unsigned beforeInvalid=Tab5Sdr::configurations;click("Recevoir");check(Tab5Sdr::configurations==beforeInvalid,"out of range ppm rejected");lv_textarea_set_text(u->ppm,"0");
 lv_dropdown_set_selected(u->band,2);lv_obj_send_event(u->band,LV_EVENT_VALUE_CHANGED,nullptr);check(!strcmp(lv_textarea_get_text(u->first),"520.000")&&!strcmp(lv_textarea_get_text(u->last),"1710.000"),"medium-wave preset explicit kHz");click("Balayer la plage");check(Tab5Sdr::scanning()&&Tab5Sdr::requestedScan.first==520000&&Tab5Sdr::requestedScan.step==9000,"real scan controls configure worker");click("Pause / reprendre");check(Tab5Sdr::scanPaused(),"pause controls scanner");click("Écoute fixe");check(!Tab5Sdr::scanning()&&Tab5Sdr::received.mode==2,"focus keeps audio mode and ends sweep");
 Tab5Sdr::output.hz=100000000;for(unsigned i=0;i<256;++i)Tab5Sdr::output.levels[i]=-90;Tab5Sdr::output.levels[128]=-20;Tab5Sdr::ready=true;Tab5SdrUi::service();lv_dropdown_set_selected(u->span,2);lv_textarea_set_text(u->offset,"10");lv_textarea_set_text(u->ymin,"-80");lv_textarea_set_text(u->ymax,"-10");click("Appliquer les axes");check(u->spanHz==60000&&u->offsetHz==10000&&u->low==-80&&u->high==-10,"axes apply displayed range");check(!strcmp(lv_label_get_text(u->xmin),"99.980000 MHz")&&!strcmp(lv_label_get_text(u->xmax),"100.040000 MHz"),"absolute X endpoints follow pan and span");
 click("Mémoriser");check(u->found.count==1&&u->dirty,"capture adds dirty detection");unsigned configBefore=Tab5Sdr::configurations;lv_dropdown_set_selected(u->detections,0);lv_obj_send_event(u->detections,LV_EVENT_VALUE_CHANGED,nullptr);check(Tab5Sdr::configurations==configBefore,"selection alone never tunes");click("Recevoir la sélection");check(Tab5Sdr::configurations==configBefore+1,"explicit listen tunes selection");
 click("Enregistrer la liste");check(!u->dirty&&disk.count(Tab5SdrUi::SavedFile)&&writes==1,"explicit save persists list once");std::string good=disk[Tab5SdrUi::SavedFile];click("Supprimer");check(u->found.count==0&&u->dirty,"delete updates model");failSave=true;click("Enregistrer la liste");check(u->dirty&&disk[Tab5SdrUi::SavedFile]==good,"write failure preserves saved list and dirty state");failSave=false;
 Tab5SdrUi::close();check(allocations.empty()&&!Tab5Sdr::busy(),"close releases UI memory and stops receiver");
 check(Tab5SdrUi::open(parent,hooks)&&Tab5SdrUi::ui->found.count==1,"saved detections load on reopen");Tab5SdrUi::close();
 disk[Tab5SdrUi::SavedFile]="TAB5-SDR,1\n531000,2,1,-40";check(Tab5SdrUi::open(parent,hooks)&&Tab5SdrUi::ui->found.count==0,"truncated file discarded as a whole");Tab5SdrUi::close();disk[Tab5SdrUi::SavedFile]=good;
 failAfter=0;check(!Tab5SdrUi::open(parent,hooks)&&allocations.empty(),"UI allocation OOM leaves no instance");failAfter=-1;
 for(unsigned i=0;i<100;++i){check(Tab5SdrUi::open(parent,hooks),"reopen");Tab5SdrUi::close();check(allocations.empty(),"open/load/close returns tracked PSRAM");}
 lv_obj_delete(parent);printf("PASS %u SDR UI checks; real LVGL actions/keyboard/axes/save failures; 100 reopen cycles\n",checks);
}
