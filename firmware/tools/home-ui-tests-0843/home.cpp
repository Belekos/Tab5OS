#include <lvgl.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cassert>
#include "worker_platform.h"
#include "production-catalogue.inc"
#include "Tab5ShellLayout.h"
#include "Tab5Focus.h"
fs::FS tab5FS;
static unsigned scriptsNext=0;static std::vector<Tab5PythonServices::AppInfo> scripts;
Tab5StorageUse::Tab5StorageUse(bool):held_(true){}
Tab5StorageUse::~Tab5StorageUse(){}
namespace Tab5UsbStorage{bool mounted(){return MockVolume::usb;}bool path(const char*p){return p&&!strncmp(p,"/USB",4)&&(p[4]=='/'||!p[4]);}uint32_t revision(){return 1;}}
namespace Tab5PythonServices{bool beginAppScan(){scriptsNext=0;return true;}int nextApp(AppInfo&out){if(scriptsNext<scripts.size()){out=scripts[scriptsNext++];return 1;}return 2;}void endAppScan(){}}
class String:public std::string{public:using std::string::string;String(const std::string&s):std::string(s){}};
static uint32_t now=0,fsRevision=1;static uint32_t millis(){return now;}uint32_t tab5FsRevision(){return fsRevision;}
static constexpr int WORK_W=1216,PAGE_H=672;
enum class Page{Home,Settings};static Page activePage=Page::Home;
static bool sdReady=true,homeAppsDirty=true,homeAppsScanning=false,homeCustomize=false,homeRenderPending=false,homePythonLibrary=false;
static bool keyboardDockFocus=false,keyboardUiNavigation=false;
static lv_obj_t *pageHost=nullptr,*homePage=nullptr,*homeNativeView=nullptr,*homePythonView=nullptr,*homeAppsGrid=nullptr,*homeDefaultGrid=nullptr,*homeCatalogueTitle=nullptr,*homeSaveButton=nullptr,*homeResetButton=nullptr,*modal=nullptr;
static lv_obj_t* homeIconBoxes[Tab5AppCatalogue::MaxApps]{};static unsigned homeIconNext=0,homeAppCount=0;
static const char*T(const char*s){return s;}static const lv_font_t*uiFont(){return &lv_font_montserrat_22;}
static lv_color_t C_BG(){return lv_color_hex(0x101010);}static lv_color_t C_SURFACE(){return lv_color_hex(0x202020);}static lv_color_t C_TEXT(){return lv_color_hex(0xeeeeee);}static lv_color_t C_MUTED(){return lv_color_hex(0xaaaaaa);}static lv_color_t C_BORDER(){return lv_color_hex(0x555555);}static lv_color_t C_ACTIVE(){return lv_color_hex(0x357ec7);}
static lv_obj_t*keyboardScope(){return modal?modal:homePage;}static void showPage(Page p){activePage=p;}static void updateTopBarForPage(){}static void setKeyboardVisible(bool){}static void toast(const char*,unsigned){}
static struct{lv_obj_t*scope(){return modal;}bool openPythonApp(const char*){return false;}bool launchExecutable(const char*,bool=false){return false;}}Tab5Tools;
static lv_obj_t*makeButton(lv_obj_t*p,const char*s,int w,int h){auto*b=lv_button_create(p);lv_obj_set_size(b,w,h);auto*l=lv_label_create(b);lv_label_set_text(l,s);lv_obj_center(l);return b;}
static lv_obj_t*makeIconButton(lv_obj_t*p,const char*s,int w,int h){return makeButton(p,s,w,h);}
#define TAB5_SYMBOL_CALCULATOR "C"
#define TAB5_SYMBOL_PYTHON "P"
static void tab5CalculatorIcon(lv_obj_t*p,lv_color_t){auto*l=lv_label_create(p);lv_label_set_text(l,"C");lv_obj_center(l);}static void tab5PythonIcon(lv_obj_t*p,lv_color_t){auto*l=lv_label_create(p);lv_label_set_text(l,"P");lv_obj_center(l);}
#include "production-home-defs.inc"
static void openHomeAction(HomeAction,bool=false){}
#include "production-home.inc"
static unsigned checks=0,flushes=0,cardDeletes=0;static uint64_t pixels=0;
static void check(bool v,const char*s){++checks;if(!v){fprintf(stderr,"FAIL %s at %u ms\n",s,now);std::abort();}}
static std::vector<uint16_t>buffer(1280*32);
static lv_display_t* display=nullptr;
static unsigned children(lv_obj_t*o){unsigned n=1;for(unsigned i=0;i<lv_obj_get_child_count(o);++i)n+=children(lv_obj_get_child(o,i));return n;}
static void step(unsigned ms=8){now+=ms;serviceHomeApps();lv_timer_handler();}
static void run(unsigned duration){const uint32_t start=now;while(uint32_t(now-start)<duration)step();}
static int find(const char*p){for(unsigned i=0;i<Tab5AppCatalogue::count();++i)if(!strcmp(Tab5AppCatalogue::at(i)->id,p))return i;return -1;}
int main(){
 lv_init();lv_tick_set_cb(millis);display=lv_display_create(1280,720);lv_display_set_buffers(display,buffer.data(),nullptr,buffer.size()*2,LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*a,uint8_t*){++flushes;pixels+=uint64_t(lv_area_get_width(a))*lv_area_get_height(a);lv_display_flush_ready(d);});
 Tab5Focus.begin(lv_screen_active(),uiFont());pageHost=lv_obj_create(lv_screen_active());lv_obj_set_pos(pageHost,64,48);lv_obj_set_size(pageHost,WORK_W,PAGE_H);lv_obj_remove_style_all(pageHost);
 MockVolume::usb=false;for(unsigned i=0;i<300;++i){char b[80];snprintf(b,sizeof(b),"/Applications/note-%03u.txt",i);MockVolume::add(b);}MockVolume::add("/Applications/alpha.elf");for(unsigned i=0;i<32;++i){char b[80];snprintf(b,sizeof(b),"/Applications/demo-%02u.elf",i);MockVolume::add(b);}
 createHomePage();run(6000);check(homeAppButtons[0],"native card created");auto*card=homeAppButtons[0];lv_obj_add_event_cb(card,[](lv_event_t*){++cardDeletes;},LV_EVENT_DELETE,nullptr);const unsigned cardCount=children(homePage),opensBefore=MockVolume::opens,nextBefore=MockVolume::nexts,flushBefore=flushes;const uint32_t revBefore=Tab5AppCatalogue::revision();const size_t nativeCatalogueBytes=MockHeap::bytes();
 run(120000);const unsigned idleOpens=MockVolume::opens-opensBefore,idleNext=MockVolume::nexts-nextBefore,idleFlushes=flushes-flushBefore;
 check(homeAppButtons[0]==card&&children(homePage)==cardCount&&!cardDeletes,"idle preserves native objects without deletion/recreation");check(flushes==flushBefore,"native idle produces no display transfer");
#if TEST_BASELINE
 check(idleOpens>0&&idleNext>0,"baseline proves repeated native catalogue IO");
#else
 check(idleOpens==0&&idleNext==0,"native idle has no catalogue IO");check(Tab5AppCatalogue::revision()==revBefore,"native idle doesn't republish table");
#endif
 homeOpenMenu(1);run(7000);check(find("/Applications/alpha.elf")>=0,"enter ELF menu publishes existing app");
 auto*alpha=homeAppButtons[find("/Applications/alpha.elf")];Tab5Focus.focusObject(alpha);lv_obj_scroll_to_y(homeNativeView,400,LV_ANIM_OFF);run(3500);const auto scroll=lv_obj_get_scroll_y(homeNativeView);check(scroll>0,"ELF library scroll fixture");run(12000);check(homeAppButtons[find("/Applications/alpha.elf")]==alpha&&Tab5Focus.focused()==alpha&&lv_obj_get_scroll_y(homeNativeView)==scroll,"no-op library scan preserves card focus and scroll");
 MockVolume::add("/Applications/new.elf");run(10000);check(find("/Applications/new.elf")>=0,"out of band ELF add appears while menu open");check(Tab5Focus.focused()==homeAppButtons[find("/Applications/alpha.elf")],"changed catalogue preserves focused key");const bool changedScrollRetained=lv_obj_get_scroll_y(homeNativeView)==scroll;
#if TEST_BASELINE
 check(!changedScrollRetained,"baseline reproduces scroll jump after passive refresh");
#else
 check(changedScrollRetained,"changed catalogue preserves viewport after focus restoration");
#endif
 auto*native=homeAppButtons[find("files")];(void)native;homeOpenMenu(0);run(500);const unsigned quietOpens=MockVolume::opens,quietNext=MockVolume::nexts;MockVolume::add("/Applications/on-return.elf");++fsRevision;run(12000);
#if !TEST_BASELINE
 check(MockVolume::opens==quietOpens&&MockVolume::nexts==quietNext,"mutations deferred while native home visible");
#endif
 homeOpenMenu(1);run(7000);check(find("/Applications/on-return.elf")>=0,"deferred change scanned on menu entry");
 // Scan cancellation on menu departure releases all filesystem handles.
 homeAppsDirty=true;step();check(homeAppsScanning,"requested scan running");homeOpenMenu(0);step();
#if !TEST_BASELINE
 check(!homeAppsScanning&&MockVolume::handles==0,"return native cancels scan and releases handles");
#endif
 homeOpenCatalogue(true);run(7000);check(homeCustomize&&find("/Applications/new.elf")>=0,"customization sees full dynamic catalogue");
 homeCancelCustomize();activePage=Page::Home;homeMenu=1;homeAppsDirty=true;step();modal=lv_obj_create(lv_layer_top());step();check(!homeAppsScanning&&MockVolume::handles==0,"modal cancels scan");lv_obj_delete(modal);modal=nullptr;
 
#if !TEST_BASELINE
 MockTask::admit=true;homeOpenMenu(1);step();homeOpenMenu(0);step();homeOpenMenu(1);step();MockTask::join();step();MockTask::join();step();
 check(!homeAppsScanning&&find("/Applications/on-return.elf")>=0,"rapid library return converges after worker cancellation");
 homeOpenMenu(0);step();MockTask::join();
#endif
 Tab5AppCatalogue::endScan();lv_obj_delete(pageHost);check(MockVolume::handles==0,"all handles released");
 printf("{\"checks\":%u,\"idleMilliseconds\":120000,\"idleFileOpens\":%u,\"idleNextFile\":%u,\"idleFlushes\":%u,\"changedScrollRetained\":%s,\"nativeObjects\":%u,\"nativeCatalogueBytes\":%zu,\"displayPixelsTotal\":%llu,\"baseline\":%s}\n",checks,idleOpens,idleNext,idleFlushes,changedScrollRetained?"true":"false",cardCount,nativeCatalogueBytes,(unsigned long long)pixels,TEST_BASELINE?"true":"false");
}
