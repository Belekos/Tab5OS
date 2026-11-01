#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <lvgl.h>
#include "Tab5MapImportPlan.h"
#include "Tab5Focus.h"
using String=std::string;
static unsigned checks=0;
static void check(bool value,const char* why){++checks;if(!value){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static uint32_t clockMs=0;static uint32_t millis(){return clockMs;}
static bool dark=true;static lv_font_t* font=nullptr;
static const char* T(const char* s){return s;}
static lv_color_t C_TEXT(){return lv_color_hex(dark?0xeeeeee:0x111111);}
static lv_color_t C_BORDER(){return lv_color_hex(dark?0x556677:0x99aabb);}
static lv_color_t C_RAISED(){return lv_color_hex(dark?0x30353b:0xf1f3f5);}
static lv_color_t C_SURFACE(){return lv_color_hex(dark?0x1f252b:0xffffff);}
static lv_color_t C_BG(){return lv_color_hex(dark?0x101215:0xffffff);}
static lv_color_t C_ACTIVE(){return lv_color_hex(0x378abb);}
static const lv_font_t* uiFont(){return font;}
static const lv_font_t* richFontFor(int,int){return font;}
static struct {int cornerRadius=12;} settings;
static lv_obj_t* root=nullptr,*mapDownloadOverlay=nullptr,*mapPlacesOverlay=nullptr,*mapPlacesList=nullptr;
static int mapSelectedPlace=-1;
static uint32_t mapGeneration=1;static unsigned mapRasterCount=0,mapRasterIndex=0;
static double mapLatitude=47.7,mapLongitude=-3.4;static uint8_t mapZoom=14;
static constexpr int SCREEN_W=1280,SCREEN_H=720,MAP_VIEW_W=1172,MAP_VIEW_H=576;
static bool sdReady=true,connected=true,storageOk=true,requestOk=true;
static std::string toastText;
static void toast(const char* text,uint32_t=1600){toastText=text;}
static bool mapDownloadsAllowed(bool* c){*c=connected;return connected;}
enum class Page{Home,Map};static Page activePage=Page::Map;
static unsigned renderCalls=0;
static void clampMapView(){}static void updateMapAttribution(){}
static void renderMapViewport(bool){++renderCalls;}
static void mapPlaceGoEvent(lv_event_t*){}static void mapPlaceRenameEvent(lv_event_t*){}
static void mapPlaceDeleteEvent(lv_event_t*){}static void mapSetHomeEvent(lv_event_t*){}
static void closeMapPlacesEvent(lv_event_t*){}static void refreshMapPlacesList(){}
struct Tab5MapTileResult {uint32_t generation=0,x=0,y=0;uint8_t zoom=0;bool success=true;int httpCode=200;};
static struct Engine {
 size_t selected=0;uint16_t rate=5;unsigned saved=0,requests=0;bool pending=false;
 Tab5MapTileResult request;
 std::set<std::tuple<unsigned,unsigned,unsigned,unsigned>> cached;
 const char* url(size_t i)const{return i==0?"https://tile.openstreetmap.org/{z}/{x}/{y}.png":i==1?"https://a.tile.opentopomap.org/{z}/{x}/{y}.png":"http://127.0.0.1:8080/{z}/{x}/{y}.png";}
 size_t selectedStyle()const{return selected;}size_t styleCount()const{return 3;}
 std::string styleName(size_t i)const{return i==0?"OpenStreetMap":i==1?"OpenTopoMap":"Serveur local";}
 uint8_t styleMaxZoom(size_t i)const{return i==1?17:19;}int8_t styleZoomShift(size_t)const{return 0;}
 uint8_t styleMinZoom(size_t)const{return 0;}uint8_t styleViewMaxZoom(size_t)const{return 22;}
 void selectStyle(size_t i){selected=i;}
 bool styleImportAllowed(size_t i)const{return Tab5MapImport::allowed(url(i));}
 uint16_t importRateTenths()const{return rate;}
 uint16_t importEffectiveRateTenths(size_t i,uint16_t preview=0)const{return Tab5MapImport::effectiveRate(preview?preview:rate,url(i));}
 bool setImportRateTenths(uint16_t v){rate=v;++saved;return true;}
 void beginGeneration(uint32_t){pending=false;}
 bool storageAvailable()const{return storageOk;}
 bool cachedTileExists(uint8_t z,uint32_t x,uint32_t y)const{return cached.count({unsigned(selected),z,x,y})!=0;}
 bool requestTile(uint8_t z,uint32_t x,uint32_t y,uint32_t g,bool region){check(region,"native import request tagged");if(!requestOk)return false;check(!pending,"only one tile in flight");pending=true;request={g,x,y,z,true,200};++requests;return true;}
 bool storeTile(const Tab5MapTileResult& r){if(!storageOk)return false;cached.insert({unsigned(selected),r.zoom,r.x,r.y});return true;}
} Tab5Map;
/* FUNCTIONS */
#include "Tab5MapImportUi.inc"
/* PLACES */
static void changed(lv_obj_t* o,int value){lv_slider_set_value(o,value,LV_ANIM_OFF);lv_obj_send_event(o,LV_EVENT_VALUE_CHANGED,nullptr);}
static void choose(unsigned value){lv_dropdown_set_selected(mapImportSource,value);lv_obj_send_event(mapImportSource,LV_EVENT_VALUE_CHANGED,nullptr);}
static void within(lv_obj_t* inner,lv_obj_t* outer,const char* why){lv_area_t a,b;lv_obj_get_coords(inner,&a);lv_obj_get_coords(outer,&b);check(a.x1>=b.x1&&a.y1>=b.y1&&a.x2<=b.x2&&a.y2<=b.y2,why);}
static lv_display_t* display;
static std::vector<unsigned char> pixels(1280*720*3),drawBuffer(1280*720*3);
static void flush(lv_display_t* d,const lv_area_t* area,uint8_t* bytes){
 for(int y=area->y1;y<=area->y2;++y){const size_t n=size_t(area->x2-area->x1+1)*3;memcpy(pixels.data()+(y*1280+area->x1)*3,bytes,n);bytes+=n;}lv_display_flush_ready(d);
}
static void capture(const char* name){
 lv_obj_update_layout(root);lv_refr_now(display);std::ofstream out(name,std::ios::binary);out<<"P6\n1280 720\n255\n";
 for(unsigned i=0;i<pixels.size();i+=3){out.put(pixels[i+2]);out.put(pixels[i+1]);out.put(pixels[i]);}
}
int main(int argc,char** argv){
 check(argc==2,"shipped font path");std::ifstream f(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});
 lv_init();display=lv_display_create(SCREEN_W,SCREEN_H);check(display!=nullptr,"actual LVGL display");
 lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,drawBuffer.data(),nullptr,drawBuffer.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,flush);
 font=lv_tiny_ttf_create_data(bytes.data(),bytes.size(),22);check(font!=nullptr,"shipped font at22");font->fallback=&lv_font_montserrat_20;
 root=lv_screen_active();lv_obj_set_style_text_font(root,font,0);Tab5Focus.begin(root,font);
 // Real planner, no duplicate tiles at dateline or low zoom. Display22 shares
 // the same native parents as the renderer instead of asking for absent z22.
 for(double lat:{-85.0,0.0,47.7,85.0})for(double lon:{-179.999,0.0,179.999})for(unsigned view:{0u,10u,19u,22u}){
   Tab5MapImport::Plan p;check(p.prepare(lat,lon,view,1172,576,view>3?view-3:0,view,19,0),"plan valid");
   check(p.count<=1024,"visible area bounded when import zoom equals view");std::set<std::tuple<unsigned,unsigned,unsigned>> unique;
   uint8_t z;uint32_t x,y;while(p.next(z,x,y)){check(z<=19&&x<(1u<<z)&&y<(1u<<z),"source tile coordinate bounds");check(unique.insert({z,x,y}).second,"dateline unique tile");}
   check(unique.size()==p.count,"estimate equals exact iterator");p.reset();size_t n=0;while(p.next(z,x,y))++n;check(n==unique.size(),"resume rewinds exactly");
 }
 Tab5MapImport::Plan huge;check(huge.prepare(0,0,0,1172,576,0,22,19,0)&&huge.count>50000,"huge estimate bounded computation");
 check(!Tab5MapImport::allowed("HTTPS://TILE.OPENSTREETMAP.ORG/1/2/3"),"OSM host comparison case insensitive");
 check(!Tab5MapImport::allowed("https://u:p@tile.openstreetmap.org.:443/1/2/3"),"OSM hostname canonicalization and credentials");
 check(Tab5MapImport::allowed("https://tile.openstreetmap.org.example.com/"),"distinct host not mistaken for OSM");
 for(unsigned rate=1;rate<=40;++rate){
   check(Tab5MapImport::effectiveRate(rate,"https://a.tile.opentopomap.org/a")==std::min(rate,5u),"Topo ceiling");
   uint32_t now=0xfffffff0,last=now;unsigned waits=0;
   const auto gap=Tab5MapImport::interval(rate);
   check(Tab5MapImport::waitForSlot(true,last,gap,[](){return true;},[&](){return now;},[&](){now+=20;++waits;}),"rate wait handles clock wrap");
   check(uint32_t(now-last)>=gap&&uint32_t(now-last)<gap+20,"exact start-to-start rate bound");
   now=last;waits=0;check(!Tab5MapImport::waitForSlot(true,last,gap,[&](){return waits<2;},[&](){return now;},[&](){now+=20;++waits;}),"rate wait cancellable");
 }
 for(unsigned cycle=0;cycle<200;++cycle){
   dark=(cycle%2)==0;Tab5Map.selected=0;mapZoom=14;activePage=Page::Map;
   mapDownloadEvent(nullptr);lv_obj_update_layout(root);check(mapDownloadOverlay&&mapImportRate,"native dialog opens without Python");
   within(mapImportRate,mapDownloadOverlay,"rate control visible inside dialog");
   check(lv_obj_has_state(mapImportStart,LV_STATE_DISABLED),"public OSM cannot bulk download");
   choose(1);changed(mapImportRate,40);lv_obj_update_layout(root);
   auto* box=lv_obj_get_child(mapDownloadOverlay,0);
   for(auto* control:{mapImportSource,mapImportLow,mapImportHigh,mapImportRate,mapImportSummary,mapImportStart,mapImportProgress,mapImportRateValue})within(control,box,"each native control fits the dialog");
   if(cycle<2)capture(cycle?"map-import-light.ppm":"map-import-dark.ppm");
   check(Tab5Map.rate==40&&strstr(lv_label_get_text(mapImportRateValue),"0.5"),"rate persisted and actual source limit shown");
   choose(2);check(!strstr(lv_label_get_text(mapImportRateValue),"Limite"),"custom source rate shown honestly");
   lv_obj_send_event(mapImportStart,LV_EVENT_CLICKED,nullptr);check(mapImportRunning,"start from UI");
   serviceMapImport(true);check(!Tab5Map.pending,"no disk/network import while storage busy");
   unsigned safety=0;while(mapImportRunning&&++safety<10000){clockMs+=20;serviceMapImport(false);if(Tab5Map.pending){auto r=Tab5Map.request;Tab5Map.pending=false;check(consumeMapImportResult(r),"production result consumed");}}
   check(!mapImportRunning&&mapImportDone==mapImportPlan.count,"all planned tiles complete");
   const auto before=Tab5Map.requests;lv_obj_send_event(mapImportStart,LV_EVENT_CLICKED,nullptr);
   safety=0;while(mapImportRunning&&++safety<10000){clockMs+=20;serviceMapImport(false);}
   check(!mapImportRunning&&Tab5Map.requests==before,"resume skips all stored tiles");
   // Exercise actual OS focus and its DELETE callbacks: Git's former reset
   // escaped tests that closed windows without dispatching their real buttons.
   auto* old=mapDownloadOverlay;
   auto* close=lv_obj_get_child(lv_obj_get_child(old,0),-3);
   if(cycle%3==0){
     Tab5Focus.focusObject(close);Tab5Focus.remember(old);
     writing::KeyEvent enter{};enter.key=writing::Key::Enter;
     check(Tab5Focus.key(enter,old),"OS keyboard activates actual Close button");
   }else if(cycle%3==1){
     Tab5Focus.focusObject(mapImportRate);Tab5Focus.remember(old);
     lv_obj_send_event(close,LV_EVENT_CLICKED,nullptr);
   }else{
     // Existing global Escape clears the pointer before deleting its saved root.
     mapDownloadOverlay=nullptr;lv_obj_delete(old);
   }
   check(!lv_obj_is_valid(old)&&!Tab5Focus.focused(),"closing removes tree and OS focus without stale reference");
   check(!mapImportRate&&!mapImportRunning&&!mapImportWaiting,"Close and Escape teardown null every widget and task state");
   serviceMapImport(false);check(!mapImportRestore,"map restored from UI service");
 }
 Tab5Map.cached.clear();Tab5Map.selected=2;mapDownloadEvent(nullptr);mapImportStartEvent(nullptr);serviceMapImport(false);
 auto r=Tab5Map.request;r.success=false;r.httpCode=429;Tab5Map.pending=false;consumeMapImportResult(r);
 check(!mapImportRunning&&strstr(mapImportMessage,"serveur"),"429 stops without automatic retry");
 const auto count=Tab5Map.requests;for(unsigned i=0;i<100;++i)serviceMapImport(false);check(Tab5Map.requests==count,"no retry after limit");
 lv_obj_delete(mapDownloadOverlay);serviceMapImport(false);
 Tab5Map.selected=2;mapDownloadEvent(nullptr);mapImportStartEvent(nullptr);serviceMapImport(false);
 check(Tab5Map.pending&&mapImportWaiting,"import in flight before Close");auto stale=Tab5Map.request;
 auto* close=lv_obj_get_child(lv_obj_get_child(mapDownloadOverlay,0),-3);
 check(std::string(lv_label_get_text(lv_obj_get_child(close,0)))=="Fermer","production Close button located");
 lv_obj_send_event(close,LV_EVENT_CLICKED,nullptr);
 check(!mapDownloadOverlay&&!mapImportWaiting&&!Tab5Map.pending,"Close cancels in-flight import and detaches pointers");
 check(!consumeMapImportResult(stale),"stale result not written into newer generation");serviceMapImport(false);
 Tab5Map.selected=2;mapDownloadEvent(nullptr);requestOk=false;mapImportStartEvent(nullptr);serviceMapImport(false);clockMs+=3001;serviceMapImport(false);
 check(!mapImportRunning&&strstr(mapImportMessage,"ressources"),"worker allocation refusal yields recoverable pause");lv_obj_delete(mapDownloadOverlay);serviceMapImport(false);requestOk=true;
 for(unsigned size:{20u,26u,32u}){
   lv_tiny_ttf_set_size(font,size);mapPlacesEvent(nullptr);lv_obj_update_layout(root);auto* box=lv_obj_get_child(mapPlacesOverlay,0);
   auto* button=lv_obj_get_child(box,5); // heading,list,go,rename,remove,setHome
   auto* label=lv_obj_get_child(button,0);check(std::string(lv_label_get_text(label))=="Définir l’accueil ici","place home action found");
   within(label,button,"saved-place home text fits at20/26/32");lv_obj_delete(mapPlacesOverlay);mapPlacesOverlay=mapPlacesList=nullptr;
 }
 lv_deinit();printf("PASS %u native import/planner/rate/actual LVGL lifecycle checks\n",checks);
}
