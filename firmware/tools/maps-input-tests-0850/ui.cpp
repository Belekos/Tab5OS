#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <set>
#include <lvgl.h>
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5UiStyle.h"
class String:public std::string {public:using std::string::string;String(const std::string& s):std::string(s){}size_t length()const{return size();}};
static unsigned checks=0;static void check(bool ok,const char* why){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static const char* T(const char* s){return s;}static const lv_font_t* uiFont(){return &lv_font_montserrat_24;}
static lv_color_t C_TEXT(){return lv_color_hex(0x202020);}static lv_color_t C_BORDER(){return lv_color_hex(0x777777);}static lv_color_t C_SURFACE(){return lv_color_hex(0xf1f0f5);}static lv_color_t C_RAISED(){return lv_color_hex(0xeeeeee);}static lv_color_t C_BG(){return lv_color_white();}static lv_color_t C_ACTIVE(){return lv_color_hex(0x99bbdd);}
static struct{int cornerRadius=8;}settings;
static constexpr int SCREEN_W=1280,SCREEN_H=720,PAGE_H=672,WORK_W=1216;
/* ENUMS */
static Page activePage=Page::Map;static KeyboardMode keyboardMode=KeyboardMode::Lower;
static lv_obj_t *root=nullptr,*keyboardPanel=nullptr,*keyboardTarget=nullptr,*editor=nullptr,*accentPopup=nullptr;
static lv_obj_t *dialogOverlay=nullptr,*dialogBox=nullptr,*dialogInput=nullptr;
static lv_obj_t *sidebarSettingsOverlay=nullptr,*fileDeleteOverlay=nullptr,*fileOperationOverlay=nullptr,*filePopupOverlay=nullptr,*wifiOverlay=nullptr,*mapExportProgress=nullptr,*mapPlacesDeleteOverlay=nullptr,*mapPlacesOverlay=nullptr,*mapStylesOverlay=nullptr,*mapSearchOverlay=nullptr,*mapDownloadOverlay=nullptr;
static TextDialogAction dialogAction=TextDialogAction::None;static String dialogDirectory,pendingRenamePath;static size_t pendingMapPlaceIndex=0;
static bool keyboardVisible=false,keyboardUiNavigation=false,keyboardDockFocus=false,mapJpegPreparing=false,mapJpegEncoding=false,smoothFontReady=false;
static unsigned keyboardNumberMode=0;static lv_font_t* keyTtfFont=nullptr;static int keyHeight=264;static int keyboardHeight(){return keyHeight;}
static bool usb=false;namespace writing {bool usbConnected(){return usb;}}
static struct{void resize(int){}}Tab5Writing,Tab5Tools;
namespace Tab5ImageEditor{void resize(int,int){}}namespace Tab5GitView{void resize(int){}}namespace Tab5Meshtastic{void resize(int){}}
namespace Tab5NumericKeyboard{unsigned mode(lv_obj_t*){return 0;}}
static void rebuildKeyboard(){lv_obj_set_size(keyboardPanel,SCREEN_W,keyHeight);lv_obj_set_pos(keyboardPanel,0,SCREEN_H-keyHeight);}static bool ensureKeyFont(){return false;}
static lv_obj_t* keyboardScope(){return dialogOverlay?dialogOverlay:root;}
static bool keyboardObjectVisible(lv_obj_t* p){return p&&!lv_obj_has_flag(p,LV_OBJ_FLAG_HIDDEN);}
static void closeTextDialog();static void setKeyboardVisible(bool,lv_obj_t* =nullptr);
static unsigned submitted=0;static String submittedText;
static void dialogOkEvent(lv_event_t*){submittedText=lv_textarea_get_text(dialogInput);++submitted;closeTextDialog();}
#define EMPTY_CLOSE(x) static void x(){}
EMPTY_CLOSE(closeSidebarSettings) EMPTY_CLOSE(closeFileDeleteDialog) EMPTY_CLOSE(closeFileOperationDialog) EMPTY_CLOSE(closeFilePopup) EMPTY_CLOSE(closeWifiPopup) EMPTY_CLOSE(closeMapPlaceDeleteOverlay)
static void closeMapPlacesEvent(lv_event_t*){}static void closeMapStylesEvent(lv_event_t*){}static void closeMapSearchEvent(lv_event_t*){}
/* BUTTON */
static lv_obj_t* makeIconButton(lv_obj_t* p,const char* t,int w,int h){auto* b=makeButton(p,t,w,h);lv_obj_set_style_text_font(lv_obj_get_child(b,0),&lv_font_montserrat_24,0);return b;}
/* KEYBOARD */
/* DIALOG */
namespace Tab5MapPrint{void cancel();}namespace Tab5Jpeg{void cancel();}
/* DISPATCH */
struct FS{std::set<std::string> paths;bool exists(const String& s){return paths.count(s);}bool mkdir(const char* s){paths.insert(s);return true;}}tab5FS;
static bool sdReady=true;static String mapJpegDestination;static unsigned mapGeneration=0,mapRasterIndex=0;static constexpr int MAP_VIEW_W=32,MAP_VIEW_H=32;static int mapZoom=10;
static struct {uint16_t pixels[1024]{};void* getBuffer(){return pixels;}}canvas;static auto* mapCanvas=&canvas;
static struct {void beginGeneration(unsigned){}const char* attribution(){return "OSM";}}Tab5Map;
namespace Tab5MapPrint{bool busy(){return false;}bool beginCapture(const uint16_t*,int,int,int,unsigned,const char*){return true;}const char* status(){return "ok";}void cancel(){}}
namespace Tab5Jpeg{bool busy(){return false;}void cancel(){}}
static void toast(const char*,int=0){}static void tab5StorageFault(const char*,const char*){}static const char* tab5StorageErrorText(){return "storage";}static void showMapExportProgress(){}
/* EXPORT */
static lv_obj_t* button(const char* label){for(unsigned i=0;i<lv_obj_get_child_count(dialogBox);++i){auto* p=lv_obj_get_child(dialogBox,i);if(!lv_obj_check_type(p,&lv_button_class))continue;auto* t=lv_obj_get_child(p,0);if(t&&!strcmp(lv_label_get_text(t),label))return p;}return nullptr;}
static void key(writing::Key k,const char* text=nullptr){writing::KeyEvent e{};e.key=k;if(text)snprintf(e.text,sizeof(e.text),"%s",text);check(dispatchDialogKeyboard(e),"dialog owns key");}
int main(){
 lv_init();auto* display=lv_display_create(SCREEN_W,SCREEN_H);static uint8_t pixels[SCREEN_W*SCREEN_H*2];lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});
 lv_display_set_theme(display,lv_theme_mono_init(display,false,uiFont()));root=lv_screen_active();Tab5Focus.begin(root,uiFont());keyboardPanel=lv_obj_create(root);rebuildKeyboard();lv_obj_add_flag(keyboardPanel,LV_OBJ_FLAG_HIDDEN);
 const uint32_t palette[9]={0xffffff,0xf1f0f5,0xeeeeee,0x202020,0x777777,0x99bbdd,0x2266aa,0xbbbbbb,0x999999};Tab5UiStyle::configure(palette,8);
 usb=true;openTextDialog(TextDialogAction::MapSearch,"Rechercher un lieu","","");auto* keys=button(LV_SYMBOL_KEYBOARD);
#if BEFORE
 check(!keys,"baseline missing explicit keyboard button reproduced");closeTextDialog();
#else
 check(keys,"manual keyboard button exists with USB present");closeTextDialog();
 for(unsigned cycle=0;cycle<100;++cycle){usb=cycle%2;keyHeight=cycle%4<2?264:354;rebuildKeyboard();openTextDialog(TextDialogAction::MapSearch,"Rechercher un lieu","","");
  check(keyboardVisible==!usb,"automatic keyboard policy unchanged");check(Tab5Focus.focused()==dialogInput&&Tab5Focus.controlling(),"map input editing focus");auto* keys=button(LV_SYMBOL_KEYBOARD);check(keys,"keyboard button found");
  if(usb)lv_obj_send_event(keys,LV_EVENT_CLICKED,nullptr);check(keyboardVisible,"keyboard usable for touch with or without USB");check(keyboardTarget==dialogInput,"keyboard target actual field");
  lv_obj_send_event(dialogInput,LV_EVENT_CLICKED,nullptr);check(keyboardVisible,"clicking field preserves manually opened keyboard");
  lv_obj_update_layout(root);lv_area_t box,keyArea,input;lv_obj_get_coords(dialogBox,&box);lv_obj_get_coords(keyboardPanel,&keyArea);lv_obj_get_coords(dialogInput,&input);check(box.y2<keyArea.y1,"dialog above normal/IME keyboard");check(input.x1>=box.x1&&input.x2<=box.x2,"input inside dialog");
  for(auto* p:{button("Annuler"),keys,button("OK")}){lv_area_t a;lv_obj_get_coords(p,&a);check(a.y2<=box.y2&&a.y1>input.y2,"action visible below input");}
  for(const char* p="Le Havre";*p;++p){char ch[2]={*p,0};key(writing::Key::Text,ch);}check(!strcmp(lv_textarea_get_text(dialogInput),"Le Havre"),"physical text edits search field");
  if(cycle%3==0){key(writing::Key::Escape);check(!dialogOverlay&&!keyboardVisible&&!keyboardTarget,"Escape closes safely");}
  else if(cycle%3==1){unsigned n=submitted;key(writing::Key::Enter);check(submitted==n+1&&submittedText=="Le Havre","physical Enter submits");check(!dialogOverlay&&!keyboardVisible&&!keyboardTarget,"Enter frees dialog");}
  else{unsigned n=submitted;lv_obj_send_event(dialogInput,LV_EVENT_READY,nullptr);check(submitted==n+1,"virtual Enter READY submits");}
 }
 tab5FS.paths={"/Pictures","/Pictures/Maps","/Pictures/Maps/map-0001.jpg","/Picture/Maps/map-0001.jpg","/Picture/Maps/map-0002.jpg.tmp"};mapExportEvent(nullptr);check(mapJpegDestination=="/Picture/Maps/map-0003.jpg","new export canonical Picture plus collision handling");check(tab5FS.exists("/Pictures/Maps/map-0001.jpg"),"old Pictures file preserved");check(tab5FS.exists("/Picture"),"canonical folder created");
#endif
 Tab5Focus.clear();lv_deinit();printf("{\"checks\":%u,\"passed\":true}\n",checks);
}
