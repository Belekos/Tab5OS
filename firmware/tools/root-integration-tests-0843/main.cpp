#include <lvgl.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <cassert>
#include <sys/time.h>
#include "Tab5ImageFormats.h"
static unsigned checks=0,invalidations=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);abort();}}while(0)
struct String {
 std::string value;static unsigned copies;
 String()=default;String(const char* s):value(s?s:""){}String(const std::string& s):value(s){}
 String(const String& s):value(s.value){++copies;}String(String&&)=default;
 String& operator=(const String& s){value=s.value;++copies;return *this;}String& operator=(String&&)=default;String& operator=(const char* s){value=s;return *this;}
 const char* c_str()const{return value.c_str();}size_t length()const{return value.size();}
 bool operator!=(const String& s)const{return value!=s.value;}bool operator==(const char* s)const{return value==s;}
 bool endsWith(const char* s)const{size_t n=strlen(s);return value.size()>=n&&value.compare(value.size()-n,n,s)==0;}
 void toLowerCase(){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return std::tolower(c);});}
 bool equalsIgnoreCase(const char* s)const{std::string v=value;std::transform(v.begin(),v.end(),v.begin(),[](unsigned char c){return std::tolower(c);});std::string rhs(s);std::transform(rhs.begin(),rhs.end(),rhs.begin(),[](unsigned char c){return std::tolower(c);});return v==rhs;}
 void replace(const char* a,const char* b){size_t p=0;while((p=value.find(a,p))!=std::string::npos){value.replace(p,strlen(a),b);p+=strlen(b);}}
 String& operator+=(const String& s){value+=s.value;return *this;}String& operator+=(const char* s){value+=s;return *this;}
 friend String operator+(String a,const char* b){a+=b;return a;}
};unsigned String::copies=0;
#include STRUCTS
struct Mutex{bool blocked=false;unsigned calls=0,held=0;int timeout=0;}mutex;
using SemaphoreHandle_t=Mutex*;constexpr int pdTRUE=1,portMAX_DELAY=-1;
int xSemaphoreTake(Mutex* m,int timeout){++m->calls;m->timeout=timeout;if(m->blocked)return 0;CHECK(!m->held);++m->held;return 1;}
void xSemaphoreGive(Mutex* m){CHECK(m->held==1);--m->held;}
class Tab5AudioEngine{public:
 SemaphoreHandle_t _snapshotMutex=&mutex;Tab5AudioSnapshot _snapshot;std::atomic<uint32_t> _indicatorRevision{1};
 bool snapshot(Tab5AudioSnapshot&)const;bool wifiEnabled()const;bool wifiConnected()const;
 bool indicatorSnapshot(Tab5AudioSnapshot&,uint32_t&,bool)const;bool wifiSnapshot(bool&,bool&)const;
}Tab5Audio;
#include METHODS
static uint32_t clockMs=1000;uint32_t millis(){return clockMs;}
static unsigned remembered=0,batteryCalls=0,layoutCalls=0,ntpConfigs=0,sntpStops=0,tzApplies=0,rtcLoads=0,rtcWrites=0,buildSets=0,dirty=0,timeUi=0,drawings=0,launches=0,keyboard=0;
static bool rtcLoadsOk=true,buildOk=true,validTime=true;
static std::string lastToast,lastPath,lastArgument,lastLanguage;
static lv_obj_t *dateLabel,*clockLabel,*rtcValue,*topWifiLabel,*topAudioIndicator,*readerClockLabel,*ntpSwitch,*timeZoneDropdown;
static uint32_t lastClockUpdateMs=0,lastTopAudioUpdateMs=0,ntpConfiguredAtMs=0;
static bool ntpServiceStarted=false,ntpAwaitingSync=false,ntpTimeReceived=false,wifiWasConnectedForRtc=false,rtcReady=true;
struct Settings{bool ntpEnabled=false;uint8_t timeZoneIndex=0;}settings;
enum class Page{Home,Editor,Settings};Page activePage=Page::Home;constexpr unsigned SCREEN_W=1280;
static const char* T(const char* text){return text;}static lv_color_t C_TEXT(){return lv_color_white();}static lv_color_t C_MUTED(){return lv_color_black();}
static String clockString(bool=false){return "12:34";}static String topBarDateString(){return "25/10/2026";}
static void updateBatteryLabel(bool){++batteryCalls;}namespace Tab5TopBarLayout{void date(lv_obj_t*,lv_obj_t*,unsigned){++layoutCalls;}}
static void audioRememberPlayback(const Tab5AudioSnapshot&){++remembered;}
static void applySelectedTimeZone(){++tzApplies;}static bool loadSystemTimeFromRtc(){++rtcLoads;return rtcLoadsOk;}
static bool systemTimeValid(){return validTime;}static bool writeRtcFromSystemTime(){++rtcWrites;return true;}
namespace m5{struct rtc_datetime_t{};}struct{struct{void setDateTime(const m5::rtc_datetime_t&){++rtcWrites;}}Rtc;}M5;
static bool parseCompileDate(m5::rtc_datetime_t&){return buildOk;}static void setSystemTimeFromDateTime(const m5::rtc_datetime_t&){++buildSets;}
static void markSettingsDirty(){++dirty;}static void refreshTimeSettingsUi(){++timeUi;}
static void toast(const char* s,int=0){lastToast=s;}
struct{void println(const char*){}}Tab5DebugSerial;
struct Zone{const char* posix;};static Zone selectedTimeZone(){return {"UTC0"};}
static void configTzTime(const char* tz,const char* a,const char* b){CHECK(!strcmp(tz,"UTC0")&&!strcmp(a,"pool.ntp.org")&&!strcmp(b,"time.cloudflare.com"));++ntpConfigs;}
static void esp_sntp_stop(){++sntpStops;}
static void (*syncCallback)(timeval*)=nullptr;static void sntp_set_time_sync_notification_cb(void(*cb)(timeval*)){syncCallback=cb;}
#define TAB5OS_HAS_SNTP_CALLBACK 1
#include UI
// Filesystem and app boundaries: extension routing is actual production code.
static String selectedPath;static bool selectedIsDir=false,selectedIsAudio=false,selectedIsVideo=false,selectedIsText=false,selectedIsImage=false;
static bool tab5ApplicationForFile(const char*){return false;}
static bool isAudioExtension(const String& s){return s.endsWith(".mp3");}static bool isVideoExtension(const String& s){return s.endsWith(".avi");}
static bool isReadableTextFile(const String& s){return s.endsWith(".txt");}static bool isEpubExtension(const String& s){return s.endsWith(".epub");}static bool isReadableImageFile(const String& s){return Tab5ImageFormats::supported(s.c_str());}
static void openFileContentDirectory(const String& s){lastPath="folder:"+s.value;}
static void openAudioPath(const String& s){lastPath="audio:"+s.value;}
static String baseNameOf(const String& s){return s;}
static void startVideoViewerSource(const String& s,const String&){lastPath="video:"+s.value;}
static void openSelectedShellScript(){lastPath="shell:"+selectedPath.value;}
static void openImageEditor(const char* s=nullptr){++drawings;lastPath=s?s:"new drawing";}
static void setKeyboardVisible(bool){++keyboard;}
static void loadDocumentFrom(const String& s){lastPath="text:"+s.value;}static void startReader(const String& s){lastPath="reader:"+s.value;}static void startImageViewer(const String& s){lastPath="image:"+s.value;}
enum class Tab5Tool{Firmware};struct{void launchExecutable(const char* s){++launches;lastPath=s;}void open(Tab5Tool,const char* s){lastPath="firmware:"+std::string(s);}void openPythonApp(const char* s){lastPath="python:"+std::string(s);}}Tab5Tools;
struct{String effectiveId(){return "fr-FR";}}Tab5Locale;
static bool tab5ExecutableRun(const char* p,const char* a,const char* l){++launches;lastPath=p;lastArgument=a?a:"";lastLanguage=l;return true;}
#include DISPATCH
static void pump(){lv_timer_handler();}
static void wifi(bool enabled,bool connected){Tab5Audio._snapshot.wifiEnabled=enabled;Tab5Audio._snapshot.wifiConnected=connected;}
int main(){
 lv_init();lv_tick_set_cb([](){return clockMs;});auto* display=lv_display_create(1280,720);static uint8_t buf[1280*32*2];lv_display_set_buffers(display,buf,nullptr,sizeof(buf),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});
 auto* screen=lv_screen_active();lv_display_add_event_cb(display,[](lv_event_t* e){if(lv_event_get_code(e)==LV_EVENT_INVALIDATE_AREA)++invalidations;},LV_EVENT_ALL,nullptr);
 dateLabel=lv_label_create(screen);clockLabel=lv_label_create(screen);rtcValue=lv_label_create(screen);topWifiLabel=lv_label_create(screen);topAudioIndicator=lv_label_create(screen);readerClockLabel=lv_label_create(screen);ntpSwitch=lv_switch_create(screen);timeZoneDropdown=lv_dropdown_create(screen);lv_dropdown_set_options(timeZoneDropdown,"UTC\nParis\nOther");
 lv_obj_add_event_cb(ntpSwitch,ntpEvent,LV_EVENT_VALUE_CHANGED,nullptr);lv_obj_add_event_cb(timeZoneDropdown,timeZoneEvent,LV_EVENT_VALUE_CHANGED,nullptr);
 updateClockLabels(true);updateTopAudioIndicator(true);pump();clockMs+=500;pump();
 String::copies=0;remembered=0;invalidations=0;
 for(unsigned t=0;t<60000;t+=8){clockMs+=8;updateTopAudioIndicator(false);pump();}
 unsigned indicatorCopies=String::copies;CHECK(lv_obj_has_flag(topAudioIndicator,LV_OBJ_FLAG_HIDDEN));CHECK(invalidations==0);
 #if NEW
 CHECK(indicatorCopies==0&&remembered==0);
 // A force requested during contention must survive even when revision is unchanged.
 activePage=Page::Home;Tab5Audio._snapshot.mode=Tab5AudioMode::Local;Tab5Audio._snapshot.nowPlaying="one\r\ntwo";++Tab5Audio._indicatorRevision;updateTopAudioIndicator(false);pump();
 CHECK(!lv_obj_has_flag(topAudioIndicator,LV_OBJ_FLAG_HIDDEN));CHECK(std::string(lv_label_get_text(topAudioIndicator)).find("one  two")!=std::string::npos);
 activePage=Page::Editor;mutex.blocked=true;updateTopAudioIndicator(true);CHECK(!lv_obj_has_flag(topAudioIndicator,LV_OBJ_FLAG_HIDDEN));mutex.blocked=false;clockMs+=8;updateTopAudioIndicator(false);CHECK(lv_obj_has_flag(topAudioIndicator,LV_OBJ_FLAG_HIDDEN));
 activePage=Page::Home;mutex.blocked=true;updateTopAudioIndicator(true);mutex.blocked=false;updateTopAudioIndicator(false);CHECK(!lv_obj_has_flag(topAudioIndicator,LV_OBJ_FLAG_HIDDEN));
 #else
 CHECK(indicatorCopies>1000&&remembered>200);
 #endif
 Tab5Audio._snapshot.mode=Tab5AudioMode::Idle;++Tab5Audio._indicatorRevision;updateTopAudioIndicator(true);pump();
 wifi(true,true);clockMs+=1000;updateClockLabels(true);pump();auto beforeColor=lv_obj_get_style_text_color(topWifiLabel,LV_PART_MAIN);auto beforeOpa=lv_obj_get_style_opa(topWifiLabel,LV_PART_MAIN);
 #if NEW
 wifi(false,false);mutex.blocked=true;clockMs+=1000;updateClockLabels(false);CHECK(lv_color_eq(beforeColor,lv_obj_get_style_text_color(topWifiLabel,LV_PART_MAIN))&&beforeOpa==lv_obj_get_style_opa(topWifiLabel,LV_PART_MAIN));mutex.blocked=false;
 #endif
 wifi(true,true);clockMs+=1000;updateClockLabels(true);pump();String::copies=0;invalidations=0;
 for(unsigned i=0;i<60;++i){clockMs+=1000;updateClockLabels(false);pump();}
 unsigned clockCopies=String::copies;CHECK(invalidations==0);
 #if NEW
 CHECK(clockCopies==0);
 #else
 CHECK(clockCopies==360);
 #endif
 // NTP: same user policy with scalar getters, no boot network when disabled.
 settings.ntpEnabled=false;ntpServiceStarted=false;String::copies=0;
 for(unsigned i=0;i<240;++i){clockMs+=250;serviceNetworkRtc();}
 unsigned ntpCopies=String::copies;CHECK(ntpConfigs==0);
 #if NEW
 CHECK(ntpCopies==0);
 #else
 CHECK(ntpCopies==1440);
 #endif
 wifi(false,false);settings.ntpEnabled=true;requestNetworkTime();CHECK(ntpConfigs==0&&!ntpServiceStarted);
 clockMs+=250;serviceNetworkRtc();wifi(true,true);clockMs+=250;serviceNetworkRtc();CHECK(ntpConfigs==1&&ntpServiceStarted&&ntpAwaitingSync&&syncCallback);
 for(unsigned i=0;i<20;++i){clockMs+=250;serviceNetworkRtc();}CHECK(ntpConfigs==1);
 unsigned writes=rtcWrites;validTime=false;syncCallback(nullptr);clockMs+=250;serviceNetworkRtc();CHECK(rtcWrites==writes&&ntpAwaitingSync);
 validTime=true;syncCallback(nullptr);clockMs+=250;serviceNetworkRtc();CHECK(rtcWrites==writes+1&&!ntpAwaitingSync&&!ntpServiceStarted);
 requestNetworkTime();wifi(true,false);clockMs+=250;serviceNetworkRtc();CHECK(!ntpServiceStarted&&!ntpAwaitingSync);
 wifi(true,true);clockMs+=250;serviceNetworkRtc();CHECK(ntpServiceStarted);lv_obj_remove_state(ntpSwitch,LV_STATE_CHECKED);lv_obj_send_event(ntpSwitch,LV_EVENT_VALUE_CHANGED,nullptr);CHECK(!settings.ntpEnabled&&!ntpServiceStarted&&lastToast=="NTP désactivé — horloge RTC");
 unsigned configs=ntpConfigs;lv_dropdown_set_selected(timeZoneDropdown,2);lv_obj_send_event(timeZoneDropdown,LV_EVENT_VALUE_CHANGED,nullptr);CHECK(settings.timeZoneIndex==0&&ntpConfigs==configs);
 lv_obj_add_state(ntpSwitch,LV_STATE_CHECKED);lv_obj_send_event(ntpSwitch,LV_EVENT_VALUE_CHANGED,nullptr);CHECK(settings.ntpEnabled&&ntpServiceStarted&&ntpConfigs==configs+1);
 lv_obj_send_event(timeZoneDropdown,LV_EVENT_VALUE_CHANGED,nullptr);CHECK(settings.timeZoneIndex==2&&ntpConfigs==configs+2&&dirty>=3&&timeUi>=3);
 stopNetworkTime();settings.ntpEnabled=false;wifi(false,false);rtcLoadsOk=false;buildOk=true;writes=rtcWrites;configs=ntpConfigs;applyClockPolicy();CHECK(buildSets==1&&rtcWrites==writes+1&&ntpConfigs==configs);
 rtcLoadsOk=true;applyClockPolicy();CHECK(buildSets==1&&!ntpServiceStarted);
 // Execute native ELF even for formerly hard-coded filename; image project stays native Dessin.
 selectedPath="/Applications/AtelierImage.app.elf";unsigned oldLaunches=launches,oldDrawings=drawings;selectedOpenEvent(nullptr);selectedExecuteEvent(nullptr);CHECK(runExecutable(selectedPath.c_str(),"/Picture/one.t5i"));
 #if NEW
 CHECK(launches==oldLaunches+3&&drawings==oldDrawings&&lastPath==selectedPath.value&&lastArgument=="/Picture/one.t5i"&&lastLanguage=="fr-FR");
 #else
 CHECK(launches==oldLaunches&&drawings==oldDrawings+3);
 #endif
 selectedPath="/Picture/one.t5i";oldDrawings=drawings;oldLaunches=launches;selectedOpenEvent(nullptr);selectedExecuteEvent(nullptr);CHECK(drawings==oldDrawings+1&&launches==oldLaunches&&lastPath==selectedPath.value);
 for(auto* p:{"/Applications/Quake.app.elf","/Applications/Duke3D.app.elf","/Applications/_custom.ELF"}){selectedPath=p;oldLaunches=launches;selectedOpenEvent(nullptr);selectedExecuteEvent(nullptr);CHECK(launches==oldLaunches+2);}
 selectedPath="/Applications/folder.elf";selectedIsDir=true;oldLaunches=launches;selectedOpenEvent(nullptr);selectedExecuteEvent(nullptr);CHECK(launches==oldLaunches&&lastPath=="folder:"+selectedPath.value);selectedIsDir=false;
 for(auto pair:{std::pair<const char*,const char*>{"/x.sh","shell:/x.sh"},{"/x.bin","firmware:/x.bin"},{"/x.py","python:/x.py"}}){selectedPath=pair.first;selectedExecuteEvent(nullptr);CHECK(lastPath==pair.second);}
 CHECK(!mutex.held);printf("{\"passed\":true,\"checks\":%u,\"indicatorIdleCopies60s\":%u,\"clockCopies60s\":%u,\"ntpDisabledCopies60s\":%u,\"ntpConfigs\":%u,\"nativeLaunches\":%u,\"drawingCalls\":%u,\"physicalDeviceTested\":false}\n",checks,indicatorCopies,clockCopies,ntpCopies,ntpConfigs,launches,drawings);
 lv_display_delete(display);lv_deinit();
}
