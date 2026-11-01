#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <lvgl.h>
#include "Tab5MapViewState.h"
using String=std::string;
static unsigned checks=0;
static void check(bool value,const char* why){++checks;if(!value){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static constexpr const char* MAP_PREFS_NAMESPACE="tab5os-map";

// NVS boundary only is simulated; all serialization/storage methods and UI
// handlers below are extracted from the actual production sources verbatim.
struct Preferences {
 static inline std::map<std::string,std::vector<uint8_t>> storage;
 static inline unsigned writes=0,opens=0;
 static inline bool readable=true,writable=true;
 bool begin(const char* name,bool=false){++opens;check(std::string(name)==MAP_PREFS_NAMESPACE,"correct NVS namespace");return readable;}
 void end(){}
 bool isKey(const char* key){return storage.count(key)!=0;}
 size_t getBytesLength(const char* key){return isKey(key)?storage[key].size():0;}
 size_t getBytes(const char* key,void* dest,size_t size){if(!isKey(key)||storage[key].size()!=size)return 0;memcpy(dest,storage[key].data(),size);return size;}
 size_t putBytes(const char* key,const void* bytes,size_t size){if(!writable)return 0;const auto* p=static_cast<const uint8_t*>(bytes);storage[key]={p,p+size};++writes;return size;}
 template<class T>T get(const char* key,T fallback){T v{};return getBytes(key,&v,sizeof(v))==sizeof(v)?v:fallback;}
 double getDouble(const char* key,double fallback){return get(key,fallback);}
 uint8_t getUChar(const char* key,uint8_t fallback){return get(key,fallback);}
 bool getBool(const char* key,bool fallback){return get(key,fallback);}
 size_t putBool(const char* key,bool value){return putBytes(key,&value,sizeof(value));}
 template<class T>static void seed(const char* key,T v){const auto* p=reinterpret_cast<const uint8_t*>(&v);storage[key]={p,p+sizeof(v)};}
 static void reset(){storage.clear();writes=opens=0;readable=writable=true;}
};
class Tab5MapEngine {
 public:
 bool _rememberView=true;
 bool rememberView()const{return _rememberView;}
 bool setRememberView(bool);
 bool loadView(double&,double&,uint8_t&);
 bool saveView(double,double,uint8_t);
 bool loadHomeView(double&,double&,uint8_t&);
 bool saveHomeView(double,double,uint8_t);
 size_t selectedStyle()const{return 0;}
 size_t styleCount()const{return 3;}
 String styleName(size_t i)const{return i==0?"OpenStreetMap":i==1?"OSM léger":"OpenTopoMap";}
 bool styleIsLightweight(size_t i)const{return i==1;}
}Tab5Map;
/* ENGINE */
static constexpr int SCREEN_W=1280,SCREEN_H=720;
static double mapLatitude=Tab5MapViewState::DefaultLatitude;
static double mapLongitude=Tab5MapViewState::DefaultLongitude;
static uint8_t mapZoom=Tab5MapViewState::DefaultZoom;
static double mapHomeLatitude=Tab5MapViewState::DefaultLatitude;
static double mapHomeLongitude=Tab5MapViewState::DefaultLongitude;
static uint8_t mapHomeZoom=Tab5MapViewState::DefaultZoom;
static std::string toastText;
static void toast(const char* s){toastText=s;}
static const char* T(const char* text){return text;}
static lv_obj_t *root=nullptr,*mapStylesOverlay=nullptr;
static bool dark=true;
static lv_color_t C_TEXT(){return lv_color_hex(dark?0xeeeeee:0x111111);}
static lv_color_t C_BORDER(){return lv_color_hex(dark?0x556677:0x99aabb);}
static lv_color_t C_RAISED(){return lv_color_hex(dark?0x30353b:0xf1f3f5);}
static lv_color_t C_SURFACE(){return lv_color_hex(dark?0x1f252b:0xffffff);}
static lv_color_t C_BG(){return lv_color_hex(dark?0x101215:0xffffff);}
static lv_color_t C_ACTIVE(){return lv_color_hex(0x378abb);}
static lv_color_t C_SELECTED(){return C_ACTIVE();}
static lv_color_t C_MUTED(){return lv_color_hex(dark?0xaabbcc:0x556677);}
static const lv_font_t* uiFont(){return LV_FONT_DEFAULT;}
static const lv_font_t* richFontFor(int,int){return uiFont();}
static struct {int cornerRadius=12;} settings;
static struct {void describe(lv_obj_t*,const char*){}}Tab5Focus;
namespace Tab5UiStyle {static void control(lv_obj_t*){}}
static void stopAudioRowMarquee(lv_obj_t* b,int w){auto* l=lv_obj_get_child(b,-1);if(l){lv_obj_set_width(l,w);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);}}
static void mapStyleSelectEvent(lv_event_t*){}
/* UI */
static void expectView(double a,double b,unsigned z,const char* message){check(mapLatitude==a&&mapLongitude==b&&mapZoom==z,message);}
static void reboot(){Tab5Map=Tab5MapEngine{};Preferences p;if(p.begin(MAP_PREFS_NAMESPACE,true)){Tab5Map._rememberView=p.getBool("remember",true);p.end();}restoreMapOpeningView();}
static void within(lv_obj_t* object,lv_obj_t* parent,const char* why){lv_area_t a,b;lv_obj_get_coords(object,&a);lv_obj_get_coords(parent,&b);check(a.x1>=b.x1&&a.y1>=b.y1&&a.x2<=b.x2&&a.y2<=b.y2,why);}
static lv_obj_t* findSwitch(lv_obj_t* parent){for(unsigned i=0;i<lv_obj_get_child_count(parent);++i){auto* child=lv_obj_get_child(parent,i);if(lv_obj_check_type(child,&lv_switch_class))return child;}return nullptr;}
int main(){
 Preferences::reset();reboot();expectView(0,0,2,"first boot shows neutral world view");
 check(Preferences::writes==0,"first boot performs no writes and no SD dependency");
 mapLatitude=12.34;mapLongitude=-56.78;mapZoom=22;saveMapLeavingView();
 check(Preferences::writes==1&&Preferences::storage["view"].size()==24,"one bounded blob on exit");
 reboot();expectView(12.34,-56.78,22,"reboot restores display zoom22 and coordinates");
 for(unsigned i=0;i<4000;++i){saveMapLeavingView();restoreMapOpeningView();}
 check(Preferences::writes==1,"4000 unchanged leave/open cycles never rewrite NVS");
 check(Tab5Map.saveHomeView(-20,30,8),"save explicit independent home");
 restoreMapOpeningView();expectView(12.34,-56.78,22,"home does not overwrite remembered view");
 check(Tab5Map.setRememberView(false),"disable remember succeeds");
 const auto disabledWrites=Preferences::writes;
 mapLatitude=35;mapLongitude=40;mapZoom=11;saveMapLeavingView();
 check(Preferences::writes==disabledWrites,"disabled option saves no coordinates");
 reboot();expectView(-20,30,8,"disabled restart selects explicit home");
 check(!Tab5Map.rememberView(),"disabled option survives reboot");
 mapLatitude=15;mapLongitude=16;mapZoom=21;
 check(Tab5Map.setRememberView(true),"reenable remember");saveMapLeavingView();reboot();
 expectView(15,16,21,"reenabled saves current view on exit, restored after reboot");
 const auto enabledWrites=Preferences::writes;
 check(Tab5Map.setRememberView(true)&&Preferences::writes==enabledWrites,"unchanged option no write");
 Preferences::writable=false;
 check(!Tab5Map.setRememberView(false)&&Tab5Map.rememberView(),"failed toggle leaves effective option unchanged");
 mapLatitude=1;mapLongitude=2;mapZoom=3;toastText.clear();saveMapLeavingView();
 check(toastText=="Dernière vue non enregistrée","failed save visible to user");
 Preferences::writable=true;reboot();expectView(15,16,21,"failed commit preserves last complete view");
 double lat=7,lon=8;uint8_t zoom=9;
 const auto original=Preferences::storage["view"];
 for(unsigned byte=0;byte<24;++byte)for(unsigned bit=0;bit<8;++bit){
  Preferences::storage["view"]=original;Preferences::storage["view"][byte]^=1<<bit;
  check(!Tab5Map.loadView(lat,lon,zoom)&&lat==7&&lon==8&&zoom==9,"corrupt bit rejected without changing caller");
 }
 for(unsigned size:{0u,1u,23u,25u,4096u}){
  Preferences::storage["view"]=original;Preferences::storage["view"].resize(size);
  check(!Tab5Map.loadView(lat,lon,zoom),"wrong record length rejected without allocation");
 }
 Preferences::storage["view"]=original;
 for(double invalid:{double(NAN),double(INFINITY),double(-INFINITY),85.1,-85.1})check(!Tab5Map.saveView(invalid,0,2),"invalid latitude rejected");
 for(double invalid:{double(NAN),double(INFINITY),double(-INFINITY),180.0,-180.1})check(!Tab5Map.saveView(0,invalid,2),"invalid longitude rejected");
 check(!Tab5Map.saveView(0,0,23),"unsupported display zoom rejected");
 for(double a:{-85.05112878,0.0,85.05112878})for(double b:{-180.0,0.0,179.999999})for(unsigned z:{0u,19u,22u}){
  check(Tab5Map.saveView(a,b,z)&&Tab5Map.loadView(lat,lon,zoom)&&lat==a&&lon==b&&zoom==z,"valid boundary exact roundtrip");
 }
 Preferences::reset();Preferences::seed("lat",10.0);Preferences::seed("lon",20.0);Preferences::seed("zoom",uint8_t(19));
 reboot();expectView(10,20,19,"legacy complete user last-view migrated read-only");
 check(Preferences::writes==0,"migration does not write on boot");saveMapLeavingView();
 check(Preferences::writes==1&&Preferences::storage.count("view"),"legacy converted once upon leaving");
 Preferences::storage["view"][4]^=1;reboot();expectView(0,0,2,"damaged current record never reverts to stale legacy coordinates");
 Preferences::reset();Preferences::seed("lat",10.0);Preferences::seed("zoom",uint8_t(19));reboot();
 expectView(0,0,2,"partial legacy record rejected");
 Preferences::seed("lon",20.0);Preferences::seed("lat",std::numeric_limits<double>::quiet_NaN());reboot();
 expectView(0,0,2,"nonfinite legacy record rejected");
 Preferences::reset();Preferences::readable=false;reboot();expectView(0,0,2,"unavailable NVS falls back safely");
 Preferences::readable=true;
 // Real LVGL event dispatch: failed persistence restores the visible switch;
 // closing uses the actual button, and list+help+switch remain inside the panel.
 lv_init();auto* display=lv_display_create(SCREEN_W,SCREEN_H);check(display!=nullptr,"actual LVGL display");root=lv_screen_active();
 for(unsigned cycle=0;cycle<200;++cycle){
  dark=(cycle%2)==0;mapStylesEvent(nullptr);lv_obj_update_layout(root);
  auto* box=lv_obj_get_child(mapStylesOverlay,0);auto* sw=findSwitch(box);check(sw!=nullptr,"remember option exists");
  for(unsigned i=0;i<lv_obj_get_child_count(box);++i)within(lv_obj_get_child(box,i),box,"settings child inside panel");
  const bool previous=Tab5Map.rememberView();
  if(previous)lv_obj_remove_state(sw,LV_STATE_CHECKED);else lv_obj_add_state(sw,LV_STATE_CHECKED);
  Preferences::writable=(cycle%3)!=0;
  lv_obj_send_event(sw,LV_EVENT_VALUE_CHANGED,nullptr);
  check(Tab5Map.rememberView()==(Preferences::writable?!previous:previous),"actual switch persists or rejects toggle correctly");
  check(lv_obj_has_state(sw,LV_STATE_CHECKED)==Tab5Map.rememberView(),"switch reflects effective persisted setting");
  auto* close=lv_obj_get_child(box,-1);lv_obj_send_event(close,LV_EVENT_CLICKED,nullptr);
  check(!mapStylesOverlay,"actual Close destroys settings dialog");Preferences::writable=true;
 }
 lv_deinit();printf("PASS %u production Cartes persistence, corruption and LVGL UI checks\n",checks);
}
