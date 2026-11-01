#include <lvgl.h>
#include <src/misc/lv_text_ap.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <memory>
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "Tab5DocumentText.h"
#include "Tab5SystemInfoView.h"
#include "Tab5MemoryLayout.h"
#include "Tab5FirmwareFootprint.h"
#include "Tab5FontDefaults.h"
#include "Tab5FilePicker.h"
#include "Tab5SettingsLayout.h"
static unsigned checks=0,flushes=0,refreshed=0,closed=0,retests=0,dirty=0;static bool saveCaptures=true;
static void check(bool c,const char* why){++checks;if(!c){fprintf(stderr,"FAIL %u: %s\n",checks,why);std::exit(1);}}
// Observe only the one explicitly expected production Snapshot allocation.
// All other LVGL allocation behavior remains real. OOM is injected only here.
static bool expectSnapshotAllocation=false,denySnapshotAllocation=false;
static void* liveSnapshot=nullptr;
static unsigned snapshotAllocations=0,snapshotFrees=0;
extern "C" void* __real_lv_malloc(size_t);
extern "C" void __real_lv_free(void*);
extern "C" void* __wrap_lv_malloc(size_t n){
 if(expectSnapshotAllocation&&n==sizeof(Tab5SystemInfoView::Snapshot)){
  expectSnapshotAllocation=false;
  if(denySnapshotAllocation)return nullptr;
  auto* p=__real_lv_malloc(n);if(p){check(!liveSnapshot,"only one modal snapshot exists");liveSnapshot=p;++snapshotAllocations;}return p;
 }
 return __real_lv_malloc(n);
}
extern "C" void __wrap_lv_free(void* p){if(p&&p==liveSnapshot){liveSnapshot=nullptr;++snapshotFrees;}__real_lv_free(p);}
static unsigned now=0;unsigned millis(){return now;}
class String:public std::string {public:using std::string::string;String(const std::string&s):std::string(s){};int lastIndexOf(char c)const{auto p=rfind(c);return p==npos?-1:int(p);}String substring(int start,int end)const{return substr(start,end-start);}bool startsWith(const char* prefix)const{return rfind(prefix,0)==0;}};
static std::vector<uint8_t> pixels(1280u*720u*3),fontData;static lv_font_t* font;static lv_display_t* display;static std::string shots;
static lv_obj_t *screen,*settingsPage,*defaultFontPopup=nullptr,*fontValue=nullptr,*fontPreview=nullptr,*defaultFontButtons[2]{},*launcher;
static constexpr int SCREEN_W=1280,SCREEN_H=720;static const char* FONT_DIR="/System";
static struct {unsigned fontIndex=1,cornerRadius=8;bool batteryInstalled=false;char interfaceFontPath[261]{},documentFontPath[261]{};}settings;
#include "popup-translations.inc"
static const char* T(const char*s){auto i=popupTranslations.find(s);return i==popupTranslations.end()?s:i->second.c_str();}static const lv_font_t* uiFont(){return font;}static const lv_font_t* uiHeadingFont(){return font;}static const lv_font_t* documentFont(){return font;}
static lv_color_t C_BG(){return lv_color_hex(Tab5UiStyle::color(0));}static lv_color_t C_SURFACE(){return lv_color_hex(Tab5UiStyle::color(1));}static lv_color_t C_RAISED(){return lv_color_hex(Tab5UiStyle::color(2));}static lv_color_t C_BORDER(){return lv_color_hex(Tab5UiStyle::color(3));}static lv_color_t C_TEXT(){return lv_color_hex(Tab5UiStyle::color(4));}static lv_color_t C_MUTED(){return lv_color_hex(Tab5UiStyle::color(5));}static lv_color_t C_ACTIVE(){return lv_color_hex(Tab5UiStyle::color(8));}
#define TAB5_SYMBOL_PRINT "print-unused"
#define TAB5_SYMBOL_LOCK "lock-unused"
static void tab5PrinterIcon(lv_obj_t*,lv_color_t,lv_color_t){}static void tab5LockIcon(lv_obj_t*,lv_color_t,lv_color_t){}
static void setKeyboardVisible(bool){}static void markSettingsDirty(){++dirty;}static void toast(const char*,unsigned=2500){}
static unsigned fontSizeForIndex(unsigned i){return Tab5DocumentText::pixels(i);}
static void fontMinusEvent(lv_event_t*){if(settings.fontIndex)--settings.fontIndex;if(fontValue)lv_label_set_text_fmt(fontValue,"%u",fontSizeForIndex(settings.fontIndex));}
static void fontPlusEvent(lv_event_t*){if(settings.fontIndex<3)++settings.fontIndex;if(fontValue)lv_label_set_text_fmt(fontValue,"%u",fontSizeForIndex(settings.fontIndex));}
namespace Tab5FontDefaults {Result inspect(const char*){return Result::Ok;}Result checkSelectionBudget(const char*,const char*){return Result::Ok;}const char* message(Result){return "Mock font validation";}}
static lv_obj_t* keyboardScope(){if(Tab5FilePicker::scope())return Tab5FilePicker::scope();if(defaultFontPopup)return defaultFontPopup;if(Tab5SystemInfoView::scope())return Tab5SystemInfoView::scope();return settingsPage;}
#include "popup-helpers.inc"
#include "Tab5DefaultFontUi.inc"
// Filesystem and directory listing adapters only. The complete FilePicker
// overlay, toolbar, callback ownership and close/focus code remain production.
namespace Tab5UsbStorage {bool mounted(){return false;}const char*status(){return "Stockage USB prêt";}}
static bool tab5SdAvailable(){return true;}
struct File{explicit operator bool()const{return true;}bool isDirectory()const{return true;}void close(){}};struct {File open(const char*){return {};}}tab5FS;
class Tab5StorageUse {public:Tab5StorageUse(bool){}explicit operator bool()const{return true;}};
static const unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
static void* heap_caps_malloc(size_t n,unsigned){return malloc(n);}static void heap_caps_free(void*p){free(p);}
static struct {const char* text(const char*s){return s;}}Tab5Locale;
namespace Tab5Files {bool canonical(const char* p,std::string& result){result=p;return true;}}
class Tab5DirectoryView {lv_obj_t* list;const lv_font_t* font;void(*chosen)(const char*,bool,void*);void* context;public:
 Tab5DirectoryView(lv_obj_t*l,const lv_font_t*f,bool(*)(const char*,bool,void*),void(*cb)(const char*,bool,void*),void*c):list(l),font(f),chosen(cb),context(c){}
 bool open(const char*){lv_obj_clean(list);auto*b=lv_button_create(list);lv_obj_set_size(b,500,54);Tab5UiStyle::button(b);auto*t=lv_label_create(b);lv_label_set_text(t,"Example-font.ttf");lv_obj_set_style_text_font(t,font,0);lv_obj_center(t);lv_obj_add_event_cb(b,[](lv_event_t*e){auto*self=static_cast<Tab5DirectoryView*>(lv_event_get_user_data(e));self->chosen("/System/Example-font.ttf",false,self->context);},LV_EVENT_CLICKED,this);Tab5Focus.focusObject(b);return true;}
 lv_obj_t* sortControl(lv_obj_t*p,int w){auto*d=lv_dropdown_create(p);lv_obj_set_size(d,w,54);lv_dropdown_set_options(d,"Nom\nDate");Tab5UiStyle::control(d);return d;}void service(){}void appearance(const lv_font_t*){}bool key(const writing::KeyEvent&){return false;}
};
#include "filepicker-production.inc"
static void pump(){now+=20;lv_tick_inc(20);lv_timer_handler();lv_obj_update_layout(screen);lv_obj_update_layout(lv_layer_top());}
static bool sameLabelText(const char*actual,const char*expected){
#if LV_USE_ARABIC_PERSIAN_CHARS
 std::string shaped(lv_text_ap_strlen(expected)+1,'\0');lv_text_ap_proc(expected,shaped.data());return !strcmp(actual,shaped.c_str());
#else
 return !strcmp(actual,expected);
#endif
}
static lv_obj_t* find(lv_obj_t*o,const char* text){if(lv_obj_check_type(o,&lv_label_class)&&sameLabelText(lv_label_get_text(o),text))return o;for(unsigned i=0;i<lv_obj_get_child_count(o);++i)if(auto*r=find(lv_obj_get_child(o,i),text))return r;return nullptr;}
static lv_obj_t* button(lv_obj_t* root,const char* s){auto*l=find(root,T(s));check(l,"button label exists");auto*b=lv_obj_get_parent(l);check(lv_obj_has_class(b,&lv_button_class),"label belongs to button");return b;}
static bool visible(lv_obj_t*o){for(;o;o=lv_obj_get_parent(o))if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return false;return true;}
static void geometry(lv_obj_t*parent){
 if(lv_obj_get_style_layout(parent,LV_PART_MAIN)==LV_LAYOUT_FLEX){auto flow=lv_obj_get_style_flex_flow(parent,LV_PART_MAIN);lv_area_t prev{};bool seen=false;for(unsigned i=0;i<lv_obj_get_child_count(parent);++i){auto*c=lv_obj_get_child(parent,i);if(!visible(c))continue;lv_area_t b;lv_obj_get_coords(c,&b);if(seen&&flow==LV_FLEX_FLOW_ROW)check(b.x1>prev.x2,"flex row siblings do not overlap");if(seen&&flow==LV_FLEX_FLOW_COLUMN)check(b.y1>prev.y2,"flex column siblings do not overlap");prev=b;seen=true;}}
 lv_area_t outer;lv_obj_get_content_coords(parent,&outer);for(unsigned i=0;i<lv_obj_get_child_count(parent);++i){auto*c=lv_obj_get_child(parent,i);if(!visible(c))continue;lv_area_t b;lv_obj_get_coords(c,&b);if(!(b.x1>=outer.x1-1&&b.x2<=outer.x2+1)){fprintf(stderr,"horizontal %d..%d in %d..%d (%s)\n",b.x1,b.x2,outer.x1,outer.x2,lv_obj_check_type(c,&lv_label_class)?lv_label_get_text(c):"widget");}check(b.x1>=outer.x1-1&&b.x2<=outer.x2+1,"popup horizontal bounds");if(!lv_obj_has_flag(parent,LV_OBJ_FLAG_SCROLLABLE)){if(!(b.y1>=outer.y1-1&&b.y2<=outer.y2+1))fprintf(stderr,"vertical %d..%d in %d..%d\n",b.y1,b.y2,outer.y1,outer.y2);check(b.y1>=outer.y1-1&&b.y2<=outer.y2+1,"popup non-scroll parent encloses child");}geometry(c);}}
static void click(lv_obj_t*b){Tab5Focus.focusObject(b);lv_obj_send_event(b,LV_EVENT_CLICKED,nullptr);pump();}
static void capture(const std::string& name){if(!saveCaptures)return;lv_obj_invalidate(screen);lv_obj_invalidate(lv_layer_top());lv_refr_now(display);FILE*f=fopen((shots+"/"+name+".ppm").c_str(),"wb");check(f,"capture opens");fprintf(f,"P6\n1280 720\n255\n");for(size_t i=0;i<pixels.size();i+=3){uint8_t rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,f);}fclose(f);}
// Execute the actual main formatter; only hardware counters are fixtures.
static unsigned memorySamples=0,footprintQueries=0;
namespace Tab5MemoryLayout {
static Snapshot fixture;
Snapshot sample(bool largest){check(!largest,"overview does not walk largest free blocks");++memorySamples;return fixture;}
}
static void resetMemoryFixture(){auto&m=Tab5MemoryLayout::fixture;m={};m.hpPhysicalBytes=768*1024;m.tcmPhysicalBytes=8*1024;m.lpPhysicalBytes=32*1024;m.l2CacheBytes=256*1024;m.hp.total=347*1024;m.hp.free=72*1024;m.tcm.total=8176;m.tcm.free=4096;m.lp.total=32752;m.lp.free=32752;m.internal.total=m.hp.total+m.tcm.total+m.lp.total;m.internal.free=m.hp.free+m.tcm.free+m.lp.free;m.psram.total=32*1024*1024;m.psram.free=14*1024*1024;m.splitAvailable=true;}
namespace Tab5FirmwareFootprint {
static Snapshot fixture{6293280,6553600,65536,true,0};
const Snapshot& snapshot(){++footprintQueries;return fixture;}
}
static struct {unsigned getChipRevision(){return 100;}unsigned getCpuFreqMHz(){return 360;}} ESP;
static constexpr const char*APP_VERSION="0.8.42";
namespace Tab5Compatibility {struct Snapshot{unsigned display=0;};Snapshot snapshot(){return {};}const char*displayName(unsigned){return "ST7123 · 1280 × 720";}}
static struct{unsigned totalMs(){return 2800;}}bootStatus;
namespace Tab5BatteryGuard {struct Sample{bool valid=false,charging=false;int millivolts=0,milliamps=0;};struct Status{Sample sample;};static Status fixture;const Status&status(){return fixture;}}
namespace Tab5Meshtastic {struct Badge{bool active=true,ready=true;};Badge badge(){return {};}}
static struct{bool wifiConnected(){return true;}bool wifiEnabled(){return true;}}Tab5Audio;
namespace Tab5UsbPrinter {bool ready(){return false;}}
static bool sdReady=true;
struct PresentStats{bool active=true;unsigned timeouts=0,copyErrors=0,publishErrors=0;};struct CacheStats{unsigned cacheErrors=0,recoveries=0;};
static PresentStats tab5DisplayPresentStats(){return {};}static CacheStats tab5DisplayCacheStats(){return {};}
#include "system-overview.inc"
static Tab5SystemInfoView::Snapshot overview(){Tab5SystemInfoView::Snapshot s;strcpy(s.heading,"Informations système · Tab5OS 0.8.42");return s;}
static bool validUtf8(const char*s){const auto*p=reinterpret_cast<const unsigned char*>(s);while(*p){unsigned c=*p++;if(c<128)continue;unsigned count=c>=0xf0?3:c>=0xe0?2:c>=0xc2?1:0;if(!count||c>0xf4)return false;unsigned value=c&((1u<<(6-count))-1);for(unsigned i=0;i<count;++i){if((p[i]&0xc0)!=0x80)return false;value=(value<<6)|(p[i]&0x3f);}p+=count;if(value>0x10ffff||(value>=0xd800&&value<=0xdfff)||(count==1&&value<128)||(count==2&&value<2048)||(count==3&&value<65536))return false;}return true;}
static void unicodeLabels(lv_obj_t*p){if(lv_obj_check_type(p,&lv_label_class))check(validUtf8(lv_label_get_text(p)),"every rendered translated label remains valid UTF-8");for(unsigned i=0;i<lv_obj_get_child_count(p);++i)unicodeLabels(lv_obj_get_child(p,i));}
static bool containsText(lv_obj_t*p,const char*text){if(lv_obj_check_type(p,&lv_label_class)&&strstr(lv_label_get_text(p),text))return true;for(unsigned i=0;i<lv_obj_get_child_count(p);++i)if(containsText(lv_obj_get_child(p,i),text))return true;return false;}
static void overviewChecks(lv_obj_t*modal){char expected[128];snprintf(expected,sizeof expected,T("%u / %u Kio utilisés"),275,347);check(find(modal,expected),"HP gauge uses HP heap only, not internal aggregate or physical 768 KiB");snprintf(expected,sizeof expected,T("%.2f / %.2f Mio occupés"),double(6293280)/(1024*1024),6.25);check(find(modal,expected),"firmware gauge shows two decimals and running partition capacity");check(containsText(modal,"768")&&containsText(modal,"256")&&containsText(modal,"165"),"physical HP/cache/nonheap values are visible");check(containsText(modal,"4096/8176")&&containsText(modal,"32752/32752"),"TCM and LP heaps retain independent exact byte values");check(find(modal,T("Mémoire du P4")),"physical memory explanation has a dedicated visible card");char complete[2048];snprintf(complete,sizeof complete,T("SRAM HP physique : %u Kio · cache L2 : %u Kio · code, données fixes et réservations : %u Kio\nSPM/TCM : %u Kio physiques · tas : %u/%u octets libres\nSRAM LP : %u Kio physiques · tas : %u/%u octets libres\nLe tas est la mémoire disponible pour les allocations. Le firmware est stocké en flash."),768,256,165,8,4096,8176,32,32752,32752);check(find(modal,complete),"complete translated memory explanation is present without truncation");unicodeLabels(modal);}

static void bars(lv_obj_t*p,std::vector<lv_obj_t*>&out){if(lv_obj_check_type(p,&lv_bar_class))out.push_back(p);for(unsigned i=0;i<lv_obj_get_child_count(p);++i)bars(lv_obj_get_child(p,i),out);}
static void modalKeyboard(const writing::KeyEvent&);
static void fontPopup(const std::string& suffix){
 snprintf(settings.interfaceFontPath,sizeof(settings.interfaceFontPath),"/System/A-very-long-custom-interface-font-file-with-many-readable-characters-0123456789.ttf");Tab5Focus.focusObject(launcher);showDefaultFontPopup();pump();check(defaultFontPopup&&Tab5Focus.focused()==defaultFontButtons[0],"font popup focuses interface selection");geometry(defaultFontPopup);capture("font-"+suffix);writing::KeyEvent tab;tab.key=writing::Key::Tab;modalKeyboard(tab);check(Tab5Focus.focused()!=defaultFontButtons[0]&&Tab5Focus.contains(defaultFontPopup,Tab5Focus.focused()),"main keyboard Tab remains inside font popup");
 auto* done=button(defaultFontPopup,"Fermer");lv_area_t box;lv_obj_get_coords(done,&box);check(box.y2<720,"font footer remains onscreen");
 click(defaultFontButtons[1]);check(Tab5FilePicker::visible()&&defaultFontPopup,"picker stacks over font dialog");pump();geometry(Tab5FilePicker::scope());capture("font-picker-"+suffix);click(button(Tab5FilePicker::scope(),"Example-font.ttf"));check(!Tab5FilePicker::visible()&&defaultFontPopup,"picker return preserves parent dialog");check(!strcmp(settings.documentFontPath,"/System/Example-font.ttf"),"actual callback updates selected role");check(Tab5Focus.focused()==defaultFontButtons[1],"picker restores source button focus");
 click(defaultFontButtons[0]);writing::KeyEvent esc;esc.key=writing::Key::Escape;check(Tab5FilePicker::key(esc)&&!Tab5FilePicker::visible()&&defaultFontPopup,"picker Escape returns to parent");
 unsigned previouslyDirty=dirty;click(button(defaultFontPopup,"Par défaut"));check(!settings.interfaceFontPath[0]&&dirty>previouslyDirty,"reset button uses actual role callback");click(done);check(!defaultFontPopup&&!fontValue&&!fontPreview&&!defaultFontButtons[0]&&!defaultFontButtons[1],"close clears all callback pointers");check(Tab5Focus.focused()==launcher,"font close restores launcher focus");
 // Page navigation can close the font modal while its independent picker is
 // still on top. This must never retain a pointer to a deleted font button.
 showDefaultFontPopup();click(defaultFontButtons[0]);closeDefaultFontPopup();pump();Tab5FilePicker::close();check(!Tab5FilePicker::visible(),"parent teardown also closes dependent picker safely");Tab5Focus.focusObject(launcher);showDefaultFontPopup();modalKeyboard(esc);check(!defaultFontPopup&&Tab5Focus.focused()==launcher,"main Escape closes font dialog and restores launcher");
}
static lv_obj_t *systemInfoOverlay=nullptr,*systemInfoBody=nullptr;
static bool systemInfoRefreshRequested=false,systemInfoDetailsRequested=false;
static void refreshSystemInfo(){++refreshed;updateSystemOverview();}
static void tab5StorageRequestRecovery(){++retests;}
#include "system-info-integration.inc"
#include "popup-keyboard.inc"
static void trackedSystemOpen(bool fail=false){check(!liveSnapshot,"previous snapshot released before reopening");expectSnapshotAllocation=true;denySnapshotAllocation=fail;showSystemInfo();check(!expectSnapshotAllocation,"production attempts exactly one snapshot cache allocation");check(fail?!liveSnapshot:!!liveSnapshot,"cache allocation or OOM follows requested fixture");}
static void systemPopup(const std::string& suffix){
 auto snapshot=overview();auto* fontLauncher=launcher;launcher=makeButton(settingsPage,"Informations système",310,48);lv_obj_set_pos(launcher,380,110);Tab5Focus.focusObject(launcher);trackedSystemOpen();pump();auto* modal=Tab5SystemInfoView::scope();check(modal&&Tab5Focus.focused()==button(modal,"Fermer"),"system popup focuses close");geometry(modal);overviewChecks(modal);capture("system-info-"+suffix);
 std::vector<lv_obj_t*> meters;bars(modal,meters);check(meters.size()==3,"three resource meters");
 snapshot.meters[0]={100,0,{}};snapshot.meters[1]={200,100,{}};snapshot.meters[2]={UINT64_MAX,UINT64_MAX,{}};Tab5SystemInfoView::update(snapshot);pump();check(lv_bar_get_value(meters[0])==0&&lv_bar_get_value(meters[1])==1000&&lv_bar_get_value(meters[2])==1000,"zero/overflow/maxuint64 gauges clamp safely");geometry(modal);unicodeLabels(modal);
 Tab5FirmwareFootprint::fixture.valid=false;updateSystemOverview();pump();check(find(modal,T("Indisponible"))&&lv_bar_get_value(meters[2])==0,"unknown firmware metadata produces unavailable gauge, never a misleading zero occupied");Tab5FirmwareFootprint::fixture.valid=true;updateSystemOverview();pump();pump();unsigned before=flushes;for(unsigned n=0;n<20;++n){updateSystemOverview();pump();}check(flushes==before,"identical system sample causes no refresh");
 click(button(modal,"Détails techniques"));check(Tab5SystemInfoView::wantsDetails(),"details opens explicitly");std::string longText;for(unsigned i=0;i<60;++i)longText+="Diagnostic: service 0 · aucun incident\n";Tab5SystemInfoView::details(longText.c_str());pump();geometry(modal);lv_area_t footer;lv_obj_get_coords(button(modal,"Fermer"),&footer);check(footer.y2<720,"long diagnostic keeps footer onscreen");lv_obj_scroll_to_y(Tab5SystemInfoView::body(),100000,LV_ANIM_OFF);pump();lv_obj_get_coords(button(modal,"Fermer"),&footer);check(footer.y2<720,"scrolling long details preserves fixed footer");capture("system-details-"+suffix);
 click(button(modal,"Masquer les détails"));check(!Tab5SystemInfoView::wantsDetails(),"details hides");unsigned prior=retests;click(button(modal,"Retester la SD"));check(retests==prior+1,"explicit SD retest delegates once");click(button(modal,"Fermer"));check(!Tab5SystemInfoView::scope()&&!Tab5SystemInfoView::body()&&Tab5Focus.focused()==launcher,"system close clears pointers and invokes restore hook");check(!systemInfoOverlay&&!systemInfoBody,"main hook clears integration aliases");check(!liveSnapshot&&snapshotAllocations==snapshotFrees,"closing popup releases its transient snapshot cache");trackedSystemOpen();writing::KeyEvent escape;escape.key=writing::Key::Escape;modalKeyboard(escape);check(!Tab5SystemInfoView::scope()&&!systemInfoOverlay&&Tab5Focus.focused()==launcher,"main Escape closes system dialog and restores launcher");check(!liveSnapshot&&snapshotAllocations==snapshotFrees,"Escape also releases snapshot cache");trackedSystemOpen(true);pump();overviewChecks(Tab5SystemInfoView::scope());closeSystemInfo();pump();check(!liveSnapshot&&snapshotAllocations==snapshotFrees,"cache OOM keeps complete rendering and closes safely");lv_obj_delete(launcher);launcher=fontLauncher;
}
int main(int argc,char**argv){check(argc==3,"font and capture directory");shots=argv[2];resetMemoryFixture();std::ifstream file(argv[1],std::ios::binary);fontData.assign(std::istreambuf_iterator<char>(file),{});lv_init();display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*,uint8_t*){++flushes;lv_display_flush_ready(d);});screen=lv_obj_create(nullptr);lv_screen_load(screen);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(screen,0,0);
 for(unsigned size:{22u,24u}){font=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),size);check(font,"TTF load");font->fallback=size==22?&lv_font_montserrat_22:&lv_font_montserrat_24;
 for(bool dark:{false,true})for(unsigned language=0;language<popupLanguageCount;++language){popupLanguage(language);saveCaptures=!strcmp(popupLanguageIds[language],"fr-FR")||!strcmp(popupLanguageIds[language],"de-DE")||!strcmp(popupLanguageIds[language],"ar");uint32_t light[]={0xf4f4f0,0xffffff,0xe8e9e6,0xb9bcb6,0x222824,0x69716a,0x427b57,0xffffff,0xd9eadc};uint32_t night[]={0x101010,0x202020,0x303030,0x666666,0xffffff,0xaaaaaa,0x357ec7,0xffffff,0x445566};Tab5UiStyle::configure(dark?night:light,8);lv_obj_set_style_bg_color(screen,C_BG(),0);lv_obj_set_style_text_font(screen,font,0);lv_obj_set_style_text_color(screen,C_TEXT(),0);Tab5Focus.begin(screen,font);Tab5Focus.theme(C_ACTIVE(),C_TEXT(),C_SURFACE());settingsPage=lv_obj_create(screen);lv_obj_remove_style_all(settingsPage);lv_obj_set_size(settingsPage,1280,720);auto* first=makeButton(settingsPage,"Autre réglage",250,48);lv_obj_set_pos(first,40,40);launcher=makeButton(settingsPage,"Polices et texte…",300,48);lv_obj_set_pos(launcher,40,110);pump();std::string suffix=std::string(popupLanguageIds[language],2)+"-"+std::string(dark?"dark-":"light-")+std::to_string(size);fontPopup(suffix);systemPopup(suffix);
 const unsigned baseline=lv_obj_get_child_count(lv_layer_top());for(unsigned i=0;i<(saveCaptures?20u:0u);++i){showDefaultFontPopup();click(defaultFontButtons[0]);closeDefaultFontPopup();pump();check(!Tab5FilePicker::visible()&&!defaultFontPopup&&lv_obj_get_child_count(lv_layer_top())==baseline,"repeated modal/picker teardown leaves no orphan overlay");}
 Tab5Focus.clear();lv_obj_delete(settingsPage);settingsPage=nullptr;
 }lv_tiny_ttf_destroy(font);}
 lv_deinit();printf("{\"passed\":true,\"checks\":%u,\"flushes\":%u,\"fonts\":[22,24],\"themes\":[\"light\",\"dark\"]}\n",checks,flushes);
}
