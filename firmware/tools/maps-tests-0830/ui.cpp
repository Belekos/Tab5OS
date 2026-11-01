#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <lvgl.h>
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5MapSearchParser.h"
class String:public std::string {public:using std::string::string;String(const std::string& x):std::string(x){}String(size_t x):std::string(std::to_string(x)){};};
static unsigned checks=0;
static void check(bool ok,const char* s){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",s);exit(1);}}
static const char* T(const char* s){return s;}
static bool dark=true;
static lv_color_t C_TEXT(){return lv_color_hex(dark?0xeeeeee:0x111111);}
static lv_color_t C_BORDER(){return lv_color_hex(dark?0x556677:0x99aabb);}
static lv_color_t C_SURFACE(){return lv_color_hex(dark?0x1f252b:0xffffff);}
static lv_color_t C_MUTED(){return lv_color_hex(0x778899);}
static lv_color_t C_RAISED(){return C_SURFACE();}
static lv_color_t C_BG(){return lv_color_hex(dark?0x101215:0xffffff);}
static lv_color_t C_ACTIVE(){return lv_color_hex(0x378abb);}
static lv_font_t* testFont=nullptr;
static const lv_font_t* uiFont(){return testFont?testFont:LV_FONT_DEFAULT;}
static const lv_font_t* richFontFor(int,int){return uiFont();}
static struct{int cornerRadius=12;}settings;
static constexpr int SCREEN_W=1280,SCREEN_H=720;
static lv_obj_t *root=nullptr,*mapSearchOverlay=nullptr;
static double mapLatitude=0,mapLongitude=0;
static uint8_t mapZoom=14,mapSearchCount=0;
static unsigned renders=0;
static void markMapViewDirty(){}static void renderMapViewport(bool){++renders;}
static void toast(const char*,int){check(false,"unexpected unavailable search");}
/* ITEM */
static Tab5MapSearchItem itemStorage[8];
static Tab5MapSearchItem* mapSearchItems=itemStorage;
/* FUNCTIONS */
static lv_obj_t* findButton(lv_obj_t* o,const char* text){if(lv_obj_has_class(o,&lv_button_class))for(unsigned i=0;i<lv_obj_get_child_count(o);++i){auto* l=lv_obj_get_child(o,i);if(lv_obj_check_type(l,&lv_label_class)&&!strcmp(lv_label_get_text(l),text))return o;}for(unsigned i=0;i<lv_obj_get_child_count(o);++i)if(auto* b=findButton(lv_obj_get_child(o,i),text))return b;return nullptr;}
static void repaint(lv_display_t* display){lv_obj_update_layout(root);lv_tick_inc(20);lv_refr_now(display);}
int main(int argc,char** argv){
 check(argc==2||argc==3,"font path supplied");std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> fontBytes{std::istreambuf_iterator<char>(file),{}};check(!fontBytes.empty(),"release SD font loaded");
 lv_init();auto* display=lv_display_create(SCREEN_W,SCREEN_H);static uint8_t pixels[SCREEN_W*SCREEN_H*2];lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});
 testFont=lv_tiny_ttf_create_data_ex(fontBytes.data(),fontBytes.size(),20,LV_FONT_KERNING_NORMAL,256);check(testFont,"release font valid");testFont->fallback=&lv_font_montserrat_20;
 root=lv_screen_active();lv_display_set_theme(display,lv_theme_mono_init(display,true,uiFont()));Tab5Focus.begin(root,uiFont());
 if(argc==3 && !strcmp(argv[2],"focus-initial")) {
  for(unsigned cycle=0;cycle<100;++cycle){
   auto* dialog=lv_obj_create(root);auto* input=lv_textarea_create(dialog);
   struct Initial {lv_obj_t* dialog;lv_obj_t* next=nullptr;bool replace;} state{dialog,nullptr,bool(cycle%2)};
   lv_obj_add_event_cb(input,[](lv_event_t* event){auto* state=static_cast<Initial*>(lv_event_get_user_data(event));lv_obj_delete(state->dialog);if(state->replace){state->next=lv_textarea_create(root);Tab5Focus.editObject(state->next);}},LV_EVENT_FOCUSED,&state);
   Tab5Focus.focusInitial(dialog,input);
   check(Tab5Focus.focused()==state.next,"initial-focus callback can destroy original field");
   check(Tab5Focus.controlling()==state.replace,"replacement field keeps editing state");
   if(state.next){Tab5Focus.clear();lv_obj_delete(state.next);}
  }
  printf("PASS %u initial-focus checks, 100 destructive FOCUSED callbacks\n",checks);return 0;
 }
 // The same focus dispatcher handles the one-line search input. READY
 // copies its text, destroys the dialog and may open another editable field.
 if(argc==3 && !strcmp(argv[2],"submit")) {
  for(unsigned cycle=0;cycle<100;++cycle){
   auto* dialog=lv_obj_create(root);auto* input=lv_textarea_create(dialog);
   lv_textarea_set_one_line(input,true);lv_textarea_set_text(input,"Saint Nazaire");
   struct Submission {lv_obj_t* dialog;lv_obj_t* next=nullptr;std::string query;} state{dialog};
   lv_obj_add_event_cb(input,[](lv_event_t* event){auto* state=static_cast<Submission*>(lv_event_get_user_data(event));state->query=lv_textarea_get_text(lv_event_get_target_obj(event));lv_obj_delete(state->dialog);state->next=lv_textarea_create(root);lv_textarea_set_one_line(state->next,true);Tab5Focus.editObject(state->next);},LV_EVENT_READY,&state);
   Tab5Focus.editObject(input);writing::KeyEvent enter{};enter.key=writing::Key::Enter;
   check(Tab5Focus.key(enter,dialog),"submit search input");
   check(state.query=="Saint Nazaire","query copied before dialog deletion");
   check(Tab5Focus.focused()==state.next&&Tab5Focus.controlling(),"new dialog keeps editing focus");
   Tab5Focus.clear();lv_obj_delete(state.next);
  }
  printf("PASS %u submit checks, 100 destroyed-dialog/replacement-input cycles\n",checks);return 0;
 }
 const char* places[]={"Saint Nazaire","New York","Le Havre","Lann Bihoué","La Rochelle","Évian les Bains","Saint-Jean de Luz","München Zentrum","東京 都","القاهرة الجديدة"};
 for(const char* place:places){
  auto* input=lv_textarea_create(root);lv_textarea_set_one_line(input,true);lv_textarea_set_max_length(input,96);lv_obj_set_style_text_font(input,uiFont(),0);lv_obj_set_size(input,610,58);Tab5Focus.editObject(input);
  for(const char* p=place;*p;++p){char byte[2]={*p,0};if(static_cast<unsigned char>(*p)<0x80){writing::KeyEvent key{};key.key=writing::Key::Text;key.text[0]=*p;check(Tab5TextInput::key(input,key),"physical ASCII input including spaces");}else {unsigned n=(static_cast<unsigned char>(*p)&0xe0)==0xc0?2:(static_cast<unsigned char>(*p)&0xf0)==0xe0?3:4;std::string cp(p,n);check(Tab5TextInput::insert(input,cp.c_str()),"touch UTF8 input");p+=n-1;}}
  if(place!=places[9])check(!strcmp(lv_textarea_get_text(input),place),"complete multiword name preserved");else check(strlen(lv_textarea_get_text(input))>0,"Arabic shaping yields visible text");repaint(display);Tab5Focus.clear();lv_obj_delete(input);
 }
 for(unsigned cycle=0;cycle<200;++cycle){dark=cycle%2==0;mapSearchCount=cycle%9;for(unsigned i=0;i<mapSearchCount;++i){auto& item=itemStorage[i];item={};Tab5MapSearch::copy(item.name,sizeof(item.name),places[(i+cycle)%10]);Tab5MapSearch::copy(item.alternative,sizeof(item.alternative),"Ville historique — nom composé, nom alternatif, quartier de la rivière");Tab5MapSearch::copy(item.displayName,sizeof(item.displayName),"Avenue de la République, quartier de la Gare, Saint Nazaire, Loire-Atlantique, Pays de la Loire, France métropolitaine, France");item.latitude=47+i;item.longitude=-2.0-i;item.zoom=14;}
  showMapSearchResults();check(mapSearchOverlay&&lv_obj_is_valid(mapSearchOverlay),"results overlay alive");repaint(display);
  if(mapSearchCount&&cycle%2){auto* box=lv_obj_get_child(mapSearchOverlay,0);auto* list=lv_obj_get_child(box,1);auto* selected=lv_obj_get_child(list,mapSearchCount-1);check(lv_obj_has_class(selected,&lv_list_button_class),"result selectable");Tab5Focus.focusObject(selected);writing::KeyEvent enter{};if(argc==3 && !strcmp(argv[2],"space")){enter.key=writing::Key::Text;enter.text[0]=' ';}else enter.key=writing::Key::Enter;check(Tab5Focus.key(enter,mapSearchOverlay),"select via physical enter or space");check(mapLatitude==47+mapSearchCount-1,"selected coordinates preserved");}
  else{auto* close=findButton(mapSearchOverlay,"Fermer");check(close,"close action exists");Tab5Focus.focusObject(close);lv_obj_send_event(close,LV_EVENT_CLICKED,nullptr);}
  check(!mapSearchOverlay,"overlay closed");repaint(display);
 }
 // Removing only the active row must retain navigation in the live list.
 for(unsigned mode=0;mode<4;++mode){
  auto* list=lv_list_create(root);auto* row=lv_list_add_button(list,nullptr,"Remove this row");auto* survivor=lv_list_add_button(list,nullptr,"Survivor");
  auto remove=[](lv_event_t* event){lv_obj_delete(lv_event_get_target_obj(event));};
  if(mode==2)Tab5Focus.activation(row,remove);else if(mode==3){lv_obj_add_flag(row,LV_OBJ_FLAG_CHECKABLE);lv_obj_add_event_cb(row,remove,LV_EVENT_VALUE_CHANGED,nullptr);}else lv_obj_add_event_cb(row,remove,mode?LV_EVENT_SINGLE_CLICKED:LV_EVENT_CLICKED,nullptr);
  Tab5Focus.focusObject(row);writing::KeyEvent key{};key.key=writing::Key::Enter;check(Tab5Focus.key(key,list),"self-deleting row activation handled");check(Tab5Focus.focused()==survivor,"surviving list receives focus");Tab5Focus.clear();lv_obj_delete(list);
 }
 // A callback can move its row out, delete its former parent and clear focus.
 {auto* list=lv_list_create(root);auto* row=lv_list_add_button(list,nullptr,"Move away");lv_obj_add_event_cb(row,[](lv_event_t* e){auto* row=lv_event_get_target_obj(e);auto* old=lv_obj_get_parent(row);lv_obj_set_parent(row,root);Tab5Focus.clear();lv_obj_delete(old);},LV_EVENT_CLICKED,nullptr);Tab5Focus.focusObject(row);writing::KeyEvent key{};key.key=writing::Key::Enter;check(Tab5Focus.key(key,list),"reparenting callback handled");check(!Tab5Focus.focused(),"deleted old parent is never restored");lv_obj_delete(row);}
 {auto* button=lv_button_create(root);unsigned activations=0;lv_obj_add_event_cb(button,[](lv_event_t* e){++*static_cast<unsigned*>(lv_event_get_user_data(e));},LV_EVENT_CLICKED,&activations);Tab5Focus.focusObject(button);writing::KeyEvent key{};key.key=writing::Key::Enter;check(Tab5Focus.key(key,root)&&activations==1&&Tab5Focus.focused()==button,"ordinary button activation retains focus");Tab5Focus.clear();lv_obj_delete(button);}
 for(bool checkbox:{false,true}){auto* control=checkbox?lv_checkbox_create(root):lv_switch_create(root);unsigned changes=0;lv_obj_add_event_cb(control,[](lv_event_t* e){++*static_cast<unsigned*>(lv_event_get_user_data(e));},LV_EVENT_VALUE_CHANGED,&changes);Tab5Focus.focusObject(control);writing::KeyEvent key{};key.key=writing::Key::Text;key.text[0]=' ';check(Tab5Focus.key(key,root)&&changes==1&&lv_obj_has_state(control,LV_STATE_CHECKED),"space toggles normal switch/checkbox");check(Tab5Focus.key(key,root)&&changes==2&&!lv_obj_has_state(control,LV_STATE_CHECKED),"second activation restores control");Tab5Focus.clear();lv_obj_delete(control);}
 Tab5Focus.clear();lv_obj_clean(root);lv_tiny_ttf_destroy(testFont);testFont=nullptr;lv_deinit();
 printf("PASS %u UI checks, 10 multiword names, 200 results cycles, actual LVGL/TinyTTF/Focus/TextInput, ASan+UBSan\n",checks);
}
