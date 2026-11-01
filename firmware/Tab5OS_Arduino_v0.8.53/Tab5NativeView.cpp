#include "Tab5NativeView.h"
#include <Arduino.h>
#include "Tab5Executable.h"
#include "Tab5ResourceManager.h"
#include "WriterUsb.h"
#include "Tab5Focus.h"
#include "Tab5AppInputQueue.h"
#include <new>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <misc/cache/instance/lv_image_cache.h>
#include <atomic>
#include <cstring>
#include <algorithm>
namespace Tab5NativeView {
namespace {
std::atomic<bool> ready{false};std::atomic<uint32_t> touchButtons{0},requests{0};
uint16_t* front=nullptr;uint16_t* back=nullptr;unsigned width=0,height=0;bool dirty=false;
static bool extendedButtons=false;
bool canvasMode=false;Tab5AppInputQueue* input=nullptr;
int canvasLeft=0,canvasTop=0,canvasWidth=0,canvasHeight=0;

uint32_t received=0,shown=0,lastStats=0;lv_obj_t* fpsLabel=nullptr;lv_obj_t* volumeSlider=nullptr;lv_obj_t* volumeLabel=nullptr;lv_obj_t* controls=nullptr;lv_obj_t* padLabel=nullptr;lv_obj_t* padConfigure=nullptr;writing::GamepadInfo padInfo;int padStep=-1;bool padReleased=false;uint8_t padMapping[8]{};uint32_t padId=0;uint8_t unmutedVolume=150;
lv_obj_t* screen=nullptr;lv_obj_t* previous=nullptr;lv_obj_t* picture=nullptr;lv_image_dsc_t image{};
StaticSemaphore_t lockStorage;SemaphoreHandle_t lock(){static auto m=xSemaphoreCreateMutexStatic(&lockStorage);return m;}
// Input events must not wait for a full PSRAM frame copy. This mutex protects
// only the bounded queue; its push/pop never calls LVGL or application code.
StaticSemaphore_t inputLockStorage;SemaphoreHandle_t inputLock(){static auto m=xSemaphoreCreateMutexStatic(&inputLockStorage);return m;}
const char* (*translate)(const char*)=nullptr;const lv_font_t*(*font)()=nullptr;
uint8_t (*getVolume)()=nullptr;void (*setVolume)(uint8_t)=nullptr;
const char* tr(const char* s){return translate?translate(s):s;}
void queueInput(const tab5_app_input_event& event){
 xSemaphoreTake(inputLock(),portMAX_DELAY);if(input)input->push(event);xSemaphoreGive(inputLock());
}
void canvasPointer(lv_event_t* e){
 auto code=lv_event_get_code(e);tab5_app_input_event event{};event.size=sizeof(event);
 if(code==LV_EVENT_PRESSED)event.type=TAB5_INPUT_DOWN;
 else if(code==LV_EVENT_PRESSING)event.type=TAB5_INPUT_MOVE;
 else if(code==LV_EVENT_RELEASED)event.type=TAB5_INPUT_UP;
 else if(code==LV_EVENT_PRESS_LOST)event.type=TAB5_INPUT_CANCEL;else return;
 auto* indev=lv_indev_active();if(!indev)return;lv_point_t point{};lv_indev_get_point(indev,&point);
 event.x=std::max(0,std::min(int(width)-1,int((point.x-canvasLeft)*int(width)/canvasWidth)));
 event.y=std::max(0,std::min(int(height)-1,int((point.y-canvasTop)*int(height)/canvasHeight)));
 queueInput(event);
}
void button(const char* text,int x,int y,int w,unsigned bit,int h=88){
 auto* b=lv_button_create(screen);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);
 auto* label=lv_label_create(b);lv_label_set_text(label,tr(text));lv_obj_center(label);
 lv_obj_add_event_cb(b,[](lv_event_t* e){auto bit=unsigned(uintptr_t(lv_event_get_user_data(e)));auto c=lv_event_get_code(e);
 if(bit>=8&&bit<=10){if(c==LV_EVENT_CLICKED){if(bit==8){if(canvasMode){tab5_app_input_event event{};event.size=sizeof(event);event.type=TAB5_INPUT_CLOSE;queueInput(event);}else tab5ExecutableStop();}else requests.fetch_or(1u<<bit);}return;}
 if(c==LV_EVENT_PRESSED)touchButtons.fetch_or(1u<<bit);
 if(c==LV_EVENT_RELEASED||c==LV_EVENT_PRESS_LOST)touchButtons.fetch_and(~(1u<<bit));
 },LV_EVENT_ALL,reinterpret_cast<void*>(uintptr_t(bit)));
}
}
void hooks(const char*(*t)(const char*),const lv_font_t*(*f)(),uint8_t(*v)(),void(*set)(uint8_t)){translate=t;font=f;getVolume=v;setVolume=set;}
void changeVolume(int value){
 value=std::max(0,std::min(255,value));if(setVolume)setVolume(value);
 if(value)unmutedVolume=value;
 if(volumeSlider)lv_slider_set_value(volumeSlider,value,LV_ANIM_OFF);
 if(volumeLabel)lv_label_set_text_fmt(volumeLabel,"%s %u %%",tr("Volume"),unsigned((value*100+127)/255));
}
void toggleControls(){if(!controls)return;if(lv_obj_has_flag(controls,LV_OBJ_FLAG_HIDDEN)){lv_obj_remove_flag(controls,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(controls);touchButtons=0;Tab5Focus.clear();}else{lv_obj_add_flag(controls,LV_OBJ_FLAG_HIDDEN);padStep=-1;}}
void updatePad(){
 if(!padLabel)return;writing::GamepadInfo info;bool connected=writing::usbGamepadInfo(info);
 if(padStep>=0){
  if(!connected||info.identity!=padId){padStep=-1;lv_label_set_text(padLabel,tr("Manette déconnectée"));return;}
  if(!info.buttons)padReleased=true;
  if(padReleased&&info.buttons&&!(info.buttons&(info.buttons-1))){unsigned code=1;while(!(info.buttons&(1u<<(code-1))))++code;padMapping[padStep++]=code;padReleased=false;
   if(padStep==(extendedButtons?8:4)){bool ok=writing::usbGamepadMap(padId,padMapping);padStep=-1;lv_label_set_text(padLabel,tr(ok?"Commandes enregistrées":"Enregistrement impossible"));return;}}
  const char* actions[]={"A","B","SELECT","START","X","Y","L","R"};lv_label_set_text_fmt(padLabel,"%s : %s",tr(padReleased?"Appuyez sur un bouton":"Relâchez les boutons"),actions[padStep]);return;
 }
 if(connected!=bool(padInfo.identity)||info.identity!=padInfo.identity||info.buttons!=padInfo.buttons||info.directions!=padInfo.directions||bool(info.reports)!=bool(padInfo.reports)||bool(info.rejected)!=bool(padInfo.rejected)){
   if(connected){
    const char* name=info.vendor==0x2dc8?(info.product==0x6001?"8BitDo SN30 Pro":"8BitDo"):tr("Manette USB");
    String pressed;const char* actions[]={"A","B","SELECT","START","X","Y","L","R"};
    auto add=[&](const char* text){if(pressed.length())pressed+=" · ";pressed+=text;};
    for(unsigned i=0;i<8;++i)if(info.mapping[i]&&info.mapping[i]<=32&&(info.buttons&(1u<<(info.mapping[i]-1))))add(actions[i]);
    if(info.directions&4)add(LV_SYMBOL_UP);if(info.directions&8)add(LV_SYMBOL_DOWN);if(info.directions&2)add(LV_SYMBOL_LEFT);if(info.directions&1)add(LV_SYMBOL_RIGHT);
    if(!info.reports)add(tr(info.rejected?"Commandes non reconnues":"Aucune commande reçue"));
    if(pressed.length())lv_label_set_text_fmt(padLabel,"%s · %s\n%s",tr("Manette connectée"),name,pressed.c_str());
    else lv_label_set_text_fmt(padLabel,"%s · %s",tr("Manette connectée"),name);
   }else lv_label_set_text(padLabel,tr("Branchez une manette USB"));padInfo=info;
 }
 if(connected)lv_obj_remove_state(padConfigure,LV_STATE_DISABLED);else lv_obj_add_state(padConfigure,LV_STATE_DISABLED);
}

void extendedControls(bool enabled){extendedButtons=enabled;}
void key(const writing::KeyEvent& e){
 using K=writing::Key;
 if(canvasMode){
  tab5_app_input_event event{};event.size=sizeof(event);event.type=TAB5_INPUT_KEY;
  switch(e.key){
   case K::Text:event.key=TAB5_KEY_TEXT;break;case K::Enter:event.key=TAB5_KEY_ENTER;break;
   case K::Escape:event.key=TAB5_KEY_ESCAPE;break;case K::Backspace:event.key=TAB5_KEY_BACKSPACE;break;
   case K::Delete:event.key=TAB5_KEY_DELETE;break;case K::Left:event.key=TAB5_KEY_LEFT;break;
   case K::Right:event.key=TAB5_KEY_RIGHT;break;case K::Up:event.key=TAB5_KEY_UP;break;
   case K::Down:event.key=TAB5_KEY_DOWN;break;case K::Tab:event.key=TAB5_KEY_TAB;break;default:return;
  }
  event.modifiers=(e.ctrl?TAB5_MOD_CTRL:0)|(e.shift?TAB5_MOD_SHIFT:0)|(e.alt?TAB5_MOD_ALT:0)|(e.meta?TAB5_MOD_META:0);
  memcpy(event.text,e.text,sizeof(event.text));event.text[sizeof(event.text)-1]=0;queueInput(event);return;
 }
 const int volume=getVolume?getVolume():150;
 if(e.key==K::Escape){if(controls&&!lv_obj_has_flag(controls,LV_OBJ_FLAG_HIDDEN)){toggleControls();return;}tab5ExecutableStop();return;}
 if(e.key==K::PageUp||e.key==K::PageDown){changeVolume(volume+(e.key==K::PageUp?13:-13));return;}
 if(e.key==K::F1){toggleControls();return;}
 if(e.key!=K::Text)return;char c=e.text[0];if(c>='A'&&c<='Z')c+='a'-'A';
 if(e.ctrl||e.meta){if(c=='s')requests.fetch_or(1u<<9);if(c=='r')requests.fetch_or(1u<<10);return;}
 if(c=='m')changeVolume(volume?0:unmutedVolume);
}
bool beginMode(unsigned w,unsigned h,bool canvas){
 if(ready.load()||!w||!h||w>(canvas?800u:320u)||h>(canvas?480u:240u))return false;
 size_t bytes=w*h*2;Tab5Resources::Budget budget{};budget.psram=bytes*2+(canvas?sizeof(Tab5AppInputQueue):0);budget.psramBlock=std::max(bytes,canvas?sizeof(Tab5AppInputQueue):size_t(0));
 auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,budget,"native video");if(!admission)return false;
 auto* a=static_cast<uint16_t*>(heap_caps_calloc(1,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 auto* b=static_cast<uint16_t*>(heap_caps_calloc(1,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 auto* q=canvas?static_cast<Tab5AppInputQueue*>(heap_caps_malloc(sizeof(Tab5AppInputQueue),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)):nullptr;
 if(!a||!b||(canvas&&!q)){heap_caps_free(a);heap_caps_free(b);heap_caps_free(q);return false;}
 if(q)new(q)Tab5AppInputQueue();input=q;canvasMode=canvas;
 admission.allocated(0,bytes*2+(canvas?sizeof(Tab5AppInputQueue):0));front=a;back=b;width=w;height=h;dirty=false;touchButtons=0;requests=0;received=shown=lastStats=0;ready.store(true,std::memory_order_release);return true;
}
bool begin(unsigned w,unsigned h){return beginMode(w,h,false);}
bool canvasBegin(unsigned w,unsigned h){return beginMode(w,h,true);}
int inputPoll(tab5_app_input_event* event){
 if(!event||event->size<sizeof(*event)||!ready.load(std::memory_order_acquire)||!canvasMode)return -1;
 xSemaphoreTake(inputLock(),portMAX_DELAY);bool got=input&&input->pop(*event);xSemaphoreGive(inputLock());return got?1:0;
}
bool present(const uint16_t* pixels,size_t count){
 if(!ready.load(std::memory_order_acquire)||!pixels||count!=width*height)return false;
 xSemaphoreTake(lock(),portMAX_DELAY);memcpy(back,pixels,count*2);dirty=true;++received;xSemaphoreGive(lock());return true;
}
std::atomic<bool> configuring{false};
uint32_t buttons(){if(configuring.load())return 0;return touchButtons.load()|(writing::usbGameButtons()&0x79ffu)|requests.exchange(0);}
bool visible(){return screen&&lv_screen_active()==screen;}
void service(){
 if(!ready.load(std::memory_order_acquire))return;
 if(!screen){
  previous=lv_screen_active();screen=lv_obj_create(nullptr);lv_obj_set_style_bg_color(screen,lv_color_hex(0x111820),0);lv_obj_set_style_text_color(screen,lv_color_hex(0xf0f0f0),0);if(font)lv_obj_set_style_text_font(screen,font(),0);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
  int sw=lv_display_get_horizontal_resolution(nullptr),sh=lv_display_get_vertical_resolution(nullptr);
  int scale=std::max(1,std::min((sw-608)/int(width),(sh-160)/int(height)));
  if(canvasMode){
   // A separate hit surface avoids ambiguity in LVGL transformed image coords.
   unsigned factor=std::min(unsigned(std::max(1,sw-16))*256/width,unsigned(std::max(1,sh-88))*256/height);
   canvasWidth=std::max(1,int(width*factor/256));canvasHeight=std::max(1,int(height*factor/256));
   canvasLeft=(sw-canvasWidth)/2;canvasTop=80+(sh-80-canvasHeight)/2;
   image.header.magic=LV_IMAGE_HEADER_MAGIC;image.header.cf=LV_COLOR_FORMAT_RGB565;image.header.w=width;image.header.h=height;image.header.stride=width*2;image.data_size=width*height*2;image.data=reinterpret_cast<uint8_t*>(front);
   picture=lv_image_create(screen);lv_image_set_src(picture,&image);lv_image_set_scale(picture,factor);lv_image_set_antialias(picture,false);
   lv_obj_set_pos(picture,canvasLeft+(canvasWidth-int(width))/2,canvasTop+(canvasHeight-int(height))/2);
   auto* hit=lv_obj_create(screen);lv_obj_remove_style_all(hit);lv_obj_set_pos(hit,canvasLeft,canvasTop);lv_obj_set_size(hit,canvasWidth,canvasHeight);lv_obj_add_flag(hit,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(hit,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(hit,canvasPointer,LV_EVENT_ALL,nullptr);
   auto* label=lv_label_create(screen);String name=tab5ExecutableSourcePath();name=name.substring(name.lastIndexOf('/')+1);name.replace(".app.elf","");lv_label_set_text(label,name.c_str());lv_obj_set_pos(label,24,22);lv_obj_set_width(label,std::max(100,sw-240));lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
   button("Quitter",sw-182,8,170,8,64);lastStats=lv_tick_get();Tab5Focus.clear();lv_screen_load(screen);
  }else{
  image.header.magic=LV_IMAGE_HEADER_MAGIC;image.header.cf=LV_COLOR_FORMAT_RGB565;image.header.w=width;image.header.h=height;image.header.stride=width*2;image.data_size=width*height*2;image.data=reinterpret_cast<uint8_t*>(front);
  picture=lv_image_create(screen);lv_image_set_src(picture,&image);lv_image_set_scale(picture,scale*256);lv_image_set_antialias(picture,false);lv_obj_center(picture);
  auto* title=lv_label_create(screen);String appTitle=tab5ExecutableSourcePath();appTitle=appTitle.substring(appTitle.lastIndexOf('/')+1);appTitle.replace(".app.elf","");appTitle.replace(".elf","");appTitle.replace("_"," ");lv_obj_set_width(title,std::max(100,sw-700));lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_CENTER,0);lv_label_set_text(title,appTitle.c_str());lv_obj_align(title,LV_ALIGN_TOP_MID,0,16);
  int cy=sh/2;button(LV_SYMBOL_UP,102,cy-142,88,2);button(LV_SYMBOL_LEFT,8,cy-48,88,1);button(LV_SYMBOL_RIGHT,196,cy-48,88,0);button(LV_SYMBOL_DOWN,102,cy+46,88,3);
  if(extendedButtons){button("X",sw-284,cy-180,112,11,84);button("Y",sw-148,cy-180,112,12,84);button("L",8,90,104,13,72);button("R",sw-120,90,104,14,72);}button("B",sw-296,cy+12,128,5,104);button("A",sw-144,cy-72,128,4,104);
  button("SELECT",sw/2-214,sh-82,190,6,68);button("START",sw/2+24,sh-82,190,7,68);
  bool engine3d=appTitle=="Doom";
  button(LV_SYMBOL_SAVE,12,8,96,9,64);Tab5Focus.describe(lv_obj_get_child(screen,lv_obj_get_child_count(screen)-1),tr("Enregistrer l’état du jeu"));
  button(engine3d?LV_SYMBOL_LIST:LV_SYMBOL_REFRESH,124,8,96,10,64);Tab5Focus.describe(lv_obj_get_child(screen,lv_obj_get_child_count(screen)-1),tr(engine3d?"Menu du jeu":"Recommencer le jeu"));
  button("Quitter",sw-182,8,170,8,64);
  lastStats=lv_tick_get();

  volumeLabel=lv_label_create(screen);lv_obj_set_pos(volumeLabel,24,sh-140);lv_obj_set_width(volumeLabel,240);
  volumeSlider=lv_slider_create(screen);lv_obj_set_pos(volumeSlider,36,sh-85);lv_obj_set_size(volumeSlider,224,24);lv_slider_set_range(volumeSlider,0,255);lv_obj_set_ext_click_area(volumeSlider,18);
  changeVolume(getVolume?getVolume():150);
  lv_obj_add_event_cb(volumeSlider,[](lv_event_t* e){changeVolume(lv_slider_get_value(lv_event_get_target_obj(e)));},LV_EVENT_VALUE_CHANGED,nullptr);
  auto* help=lv_button_create(screen);lv_obj_set_pos(help,sw-284,sh-84);lv_obj_set_size(help,268,66);auto* hl=lv_label_create(help);lv_label_set_text(hl,tr("Clavier · F1"));lv_obj_center(hl);
  lv_obj_add_event_cb(help,[](lv_event_t*){writing::KeyEvent e;e.key=writing::Key::F1;key(e);},LV_EVENT_CLICKED,nullptr);
  controls=lv_obj_create(screen);lv_obj_set_size(controls,std::min(960,sw-32),std::min(590,sh-32));lv_obj_center(controls);lv_obj_set_style_bg_color(controls,lv_color_hex(0x111820),0);lv_obj_set_style_bg_opa(controls,LV_OPA_COVER,0);lv_obj_set_style_text_color(controls,lv_color_hex(0xf0f0f0),0);lv_obj_set_style_pad_all(controls,22,0);lv_obj_set_scroll_dir(controls,LV_DIR_VER);
  auto* helpText=lv_label_create(controls);lv_obj_set_width(helpText,lv_pct(100));lv_label_set_long_mode(helpText,LV_LABEL_LONG_WRAP);
  lv_label_set_text(helpText,tr(engine3d?"Flèches : déplacement · Z/W : A · X : B\nS : X · A/Q : Y · Q/A : L · E : R\nEntrée : Start · Retour arrière : Select\nCtrl+R : menu du jeu · Ctrl+S : sauvegarde rapide\nPage haut/bas : volume · M : muet\nÉchap : quitter · F1 : masquer l’aide":appTitle=="Maze3D"?"Ramassez les trois clés jaunes puis rejoignez la sortie verte.\nFlèches : avancer, reculer et tourner · X/B : courir\nRetour arrière / Select : plan · Entrée / Start : nouveau labyrinthe\nPage haut/bas : volume · Ctrl+S : sauvegarder\nÉchap : quitter · F1 : masquer l’aide":"Flèches : direction · Z/W : A · X : B\nS : X · A/Q : Y · Q/A : L · E : R\nEntrée : Start · Retour arrière : Select\nPage haut/bas : volume · M : muet\nCtrl+S : sauvegarder · Ctrl+R : recommencer\nÉchap : quitter · F1 : masquer l’aide"));
  padLabel=lv_label_create(controls);lv_obj_set_pos(padLabel,0,230);lv_obj_set_width(padLabel,lv_pct(100));lv_label_set_text(padLabel,tr("Branchez une manette USB"));
  padConfigure=lv_button_create(controls);lv_obj_set_pos(padConfigure,0,312);lv_obj_set_size(padConfigure,lv_pct(100),64);auto* cl=lv_label_create(padConfigure);lv_label_set_text(cl,tr("Configurer la manette"));lv_obj_center(cl);
  lv_obj_add_event_cb(padConfigure,[](lv_event_t*){writing::GamepadInfo i;if(writing::usbGamepadInfo(i)){padId=i.identity;padStep=0;padReleased=false;memcpy(padMapping,i.mapping,sizeof(padMapping));}},LV_EVENT_CLICKED,nullptr);
  auto* done=lv_button_create(controls);lv_obj_set_pos(done,0,394);lv_obj_set_size(done,lv_pct(100),64);auto* dl=lv_label_create(done);lv_label_set_text(dl,tr("Fermer"));lv_obj_center(dl);lv_obj_add_event_cb(done,[](lv_event_t*){toggleControls();},LV_EVENT_CLICKED,nullptr);
  lv_obj_add_flag(controls,LV_OBJ_FLAG_HIDDEN);padStep=-1;padInfo={};
  Tab5Focus.clear();lv_screen_load(screen);
  }
 }
 if(controls&&!lv_obj_has_flag(controls,LV_OBJ_FLAG_HIDDEN)){updatePad();configuring=padStep>=0;}else configuring=false;
 static uint32_t painted=0;uint32_t now=lv_tick_get();if(uint32_t(now-painted)<16)return;
 // A producer can be copying 768 KiB. Keep the pending frame and retry on
 // the next UI pass instead of suspending input/display until the copy ends.
 if(xSemaphoreTake(lock(),0)!=pdTRUE)return;
 const bool changed=dirty;const bool statsDue=uint32_t(now-lastStats)>=1000;
 uint32_t frames=0;
 if(changed){std::swap(front,back);image.data=reinterpret_cast<uint8_t*>(front);dirty=false;}
 if(statsDue){frames=received;received=0;}
 xSemaphoreGive(lock());
 // Only the UI owns front until the next swap; the producer writes back.
 // LVGL calls do not need to hold the frame mutex.
 if(changed){lv_image_set_src(picture,&image);lv_obj_invalidate(picture);painted=now;++shown;}
 if(statsDue){char text[64];snprintf(text,sizeof(text),"%u / %u %s",unsigned(frames*1000/(now-lastStats)),unsigned(shown*1000/(now-lastStats)),tr("images/s"));if(fpsLabel)lv_label_set_text(fpsLabel,text);shown=0;lastStats=now;}
}
void close(){
 ready.store(false,std::memory_order_release);
 if(screen){if(lv_screen_active()==screen&&previous&&lv_obj_is_valid(previous))lv_screen_load(previous);Tab5Focus.clear();lv_obj_delete(screen);screen=picture=previous=fpsLabel=volumeSlider=volumeLabel=controls=padLabel=padConfigure=nullptr;padStep=-1;configuring=false;}
 // A decoder may keep a converted/aligned frame under this static source
 // descriptor. Drop it before returning its backing storage to the heap.
 lv_image_cache_drop(&image);image={};
 heap_caps_free(front);heap_caps_free(back);front=back=nullptr;if(input){input->~Tab5AppInputQueue();heap_caps_free(input);input=nullptr;}canvasMode=false;touchButtons=0;width=height=0;extendedButtons=false;
}
}
