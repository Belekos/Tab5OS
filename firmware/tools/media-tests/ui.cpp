#include "Platform.h"
#include <lvgl.h>
#include "Tab5TextInput.cpp"
#include "Tab5UiStyle.cpp"
#define private public
#include "Tab5MediaList.h"
#undef private
#include "Tab5MediaList.cpp"
#include "Tab5MediaSession.cpp"
#include "Tab5Search.h"
#include <set>
#include <cstdio>
enum class AudioView:uint8_t{Local,Radio,Recordings,Videos};
enum class Page:uint8_t{Home,Audio};static Page activePage=Page::Audio;
enum class TextDialogAction:uint8_t{NewFolder};
enum class Tab5AudioMode:uint8_t{Idle,Local,Radio,Recording};
struct Tab5AudioStation{String name,style,url;};
struct Tab5AudioSnapshot{Tab5AudioMode mode=Tab5AudioMode::Idle;bool recording=false;uint8_t volume=150;String currentPath,currentStationUrl,nowPlaying;};
struct Tab5VideoItem{String name,source;bool remote=false;};
struct Audio {
 std::vector<Tab5AudioStation> stations;std::vector<String> tracks;
 Tab5AudioMode mode=Tab5AudioMode::Idle;unsigned requests=0,selected=0;
 void ensureLibraryLoaded(){}void reload(){}
 size_t stationCount(){return stations.size();}bool station(size_t i,Tab5AudioStation& s){if(i>=stations.size())return false;s=stations[i];return true;}
 size_t trackCount(){return tracks.size();}String trackPath(size_t i){return i<tracks.size()?tracks[i]:String();}String trackName(size_t i){String p=trackPath(i);return p.substring(p.lastIndexOf('/')+1);}
 bool snapshot(Tab5AudioSnapshot& s){s.mode=mode;return true;}
 bool playStation(size_t i){++requests;selected=i;mode=Tab5AudioMode::Radio;return true;}
 bool playTrack(size_t i){++requests;selected=i;mode=Tab5AudioMode::Local;return true;}
 void togglePause(){}void setVolume(uint8_t v){Tab5MediaSession::volume(v,millis());}
} Tab5Audio;
struct Video{void ensureLibraryLoaded(){}void reload(){}void stop(){}size_t count(){return 0;}bool item(size_t,Tab5VideoItem&){return false;}}Tab5Video;
namespace Tab5Favorites {
std::set<std::string> values[2];bool contains(bool radio,const String& id){return values[radio].count(id.c_str())!=0;}
bool toggle(bool radio,const String& id){if(!values[radio].erase(id.c_str()))values[radio].insert(id.c_str());return true;}
}
static constexpr int WORK_W=1216,PAGE_H=672;
static lv_obj_t* pageHost=nullptr,*audioPage=nullptr;
static struct{uint8_t audioVolume=150;}settings;
static String deletedPath;
static const char* T(const char* p){return p;}
static const lv_font_t* uiFont(){return &lv_font_montserrat_24;}static const lv_font_t* uiHeadingFont(){return uiFont();}
static lv_color_t C_BG(){return lv_color_hex(Tab5UiStyle::color(0));}static lv_color_t C_SURFACE(){return lv_color_hex(Tab5UiStyle::color(1));}
static lv_color_t C_BORDER(){return lv_color_hex(Tab5UiStyle::color(3));}static lv_color_t C_TEXT(){return lv_color_hex(Tab5UiStyle::color(4));}
static lv_color_t C_MUTED(){return lv_color_hex(Tab5UiStyle::color(5));}static lv_color_t C_RAISED(){return lv_color_hex(Tab5UiStyle::color(2));}static lv_color_t C_ACTIVE(){return lv_color_hex(Tab5UiStyle::color(8));}
namespace Tab5Shell{void width(lv_obj_t*,int){}}
static String baseNameOf(const String& p){return p.substring(p.lastIndexOf('/')+1);}
static String parentDirOf(const String& p){int at=p.lastIndexOf('/');return at<=0?String("/"):p.substring(0,at);}
static String joinPath(const String& a,const String& b){return a+"/"+b;}
static bool audioPathIsRecording(const String& p){return p.startsWith("/Recordings/");}
static String audioAlbumName(const String& p){return baseNameOf(p);}
static String audioStationCategory(const Tab5AudioStation& station){return station.style.length()?station.style:String("Autres");}
static void addUniqueAudioGroup(std::vector<String>& groups,const String& value){for(auto& group:groups)if(group==value)return;groups.push_back(value);}
static void sortAudioGroups(std::vector<String>& groups){std::sort(groups.begin(),groups.end(),[](const String&a,const String&b){return a.compareTo(b)<0;});}
static void updateAudioUi(bool){}static void showPage(Page){}static void showPythonLibrary(bool){}static void startVideoViewer(size_t){}
static void toast(const char*){}static void askDeletePath(const String& path){deletedPath=path;}
static void openTextDialog(TextDialogAction,const char*,const char*,const String&){}
static lv_obj_t* makeIconButton(lv_obj_t* parent,const char* text,int w,int h){auto* b=lv_button_create(parent);lv_obj_set_size(b,w,h);auto* label=lv_label_create(b);lv_label_set_text(label,text);lv_obj_center(label);return b;}
static lv_obj_t* makeMicrophoneButton(lv_obj_t* parent,int w,int h,lv_obj_t** graphic){auto* b=makeIconButton(parent,LV_SYMBOL_AUDIO,w,h);if(graphic)*graphic=lv_obj_get_child(b,0);return b;}
static lv_obj_t* makeSearchButton(lv_obj_t* p){return makeIconButton(p,LV_SYMBOL_EYE_OPEN,42,36);}
static void styleSettingsSlider(lv_obj_t* p){Tab5UiStyle::control(p);}
static void audioSearchEvent(lv_event_t*){}static void audioPreviousEvent(lv_event_t*){}static void audioNextEvent(lv_event_t*){}
static void audioStopEvent(lv_event_t*){}static void audioRecordEvent(lv_event_t*){}static void audioProgressEvent(lv_event_t*){}
static void refreshAudioList();static void audioRememberBrowser();static void audioRememberScroll();static void audioRememberItem(size_t);
#include "ui-production.inc"
static unsigned checks=0;
static void check(bool result,const char* what){if(!result){fprintf(stderr,"FAIL %s\n",what);exit(1);}++checks;}
static void click(lv_obj_t* target){lv_obj_send_event(target,LV_EVENT_PRESSED,nullptr);lv_obj_send_event(target,LV_EVENT_CLICKED,nullptr);lv_obj_update_layout(lv_screen_active());}
int main(){
 lv_init();lv_display_create(1280,720);auto* screen=lv_screen_active();Tab5Focus.begin(screen,uiFont());
 uint32_t palette[]={0x101010,0x202020,0x303030,0x777777,0xffffff,0x999999,0x55aaff,0xaaaaaa,0x345678};Tab5UiStyle::configure(palette,12);
 pageHost=lv_obj_create(screen);lv_obj_set_size(pageHost,WORK_W,PAGE_H);lv_obj_set_style_pad_all(pageHost,0,0);lv_obj_set_style_text_font(pageHost,uiFont(),0);
 for(unsigned i=0;i<600;++i){char name[40],url[80];snprintf(name,sizeof(name),"Station %03u",i);snprintf(url,sizeof(url),"https://example/%u",i);Tab5Audio.stations.push_back({name,"FR/news",url});}
 Tab5Audio.tracks={"/Audio/Album/track.mp3","/Recordings/record.wav"};Fake::metadata["/Audio/Album/track.mp3"]={10,1};Fake::metadata["/Recordings/record.wav"]={15,2};
 Tab5Favorites::values[1]={"https://example/77","https://example/88"};
 Tab5MediaSession::begin(93);Tab5MediaSession::browser(1,2,"/Audio/Album","FR/news",10);Tab5MediaSession::select(1,"https://example/77","Station 077",10);Tab5MediaSession::flush(11);Tab5MediaSession::begin(150);
 audioRestoreBrowser();createAudioPage();refreshAudioList();lv_obj_update_layout(screen);
 check(Tab5Audio.requests==0,"actual restore/page construction/list refresh do not autoplay");check(audioView==AudioView::Radio&&audioFavoritesOnly[1]&&audioRows->count()==2,"actual callbacks restore favorites radio page");
 check(lv_dropdown_get_selected(audioFavoritesSelector)==1&&lv_obj_has_state(audioFavoritesTab,LV_STATE_CHECKED),"star and selector reflect persisted favorites");
 check(lv_obj_get_child_count(audioList)==25,"actual main list remains pool24");
 lv_area_t star,selector,sort,list;lv_obj_get_coords(audioFavoritesTab,&star);lv_obj_get_coords(audioFavoritesSelector,&selector);lv_obj_get_coords(audioSortSelector,&sort);lv_obj_get_coords(audioList,&list);
 check(sort.x2<selector.x1&&sort.y2<list.y1&&selector.y2<list.y1,"sort/favorite dropdowns stay above scrolling list without overlap");
 check(selector.x2<=790&&selector.x1>=190&&star.x2<selector.x1,"favorite dropdown fits library area at full24px font");
 lv_area_t library;lv_obj_get_content_coords(lv_obj_get_parent(audioList),&library);check(list.x1>=library.x1&&list.x2<=library.x2&&list.y2<=library.y2,"media list is fully inside library content without bottom clipping");check(selector.x2<=library.x2,"favorite selector avoids library border clipping");
 check(lv_dropdown_get_option_count(audioSortSelector)==2,"radio sort offers only meaningful name orders");
 click(audioPlayBtn);check(Tab5Audio.requests==1&&Tab5Audio.selected==77,"actual Play chooses restored station identity");Tab5Audio.mode=Tab5AudioMode::Idle;
 lv_dropdown_set_selected(audioFavoritesSelector,0);lv_obj_send_event(audioFavoritesSelector,LV_EVENT_VALUE_CHANGED,nullptr);check(audioRows->count()==601&&!audioFavoritesOnly[1],"actual dropdown displays all600stations");
 lv_dropdown_set_selected(audioSortSelector,1);lv_obj_send_event(audioSortSelector,LV_EVENT_VALUE_CHANGED,nullptr);check(audioRows->entries_[0].kind==Tab5MediaList::Parent&&audioRows->entries_[1].index==599,"actual radio sort changes logical order");
 click(audioRows->slots_[1].row);check(Tab5Audio.requests==2&&Tab5Audio.selected==599,"actual row callback plays sorted station");
 click(audioFavoritesTab);check(audioFavoritesOnly[1]&&audioRows->count()==2,"native star button restores favorite list");
 click(audioRecordTab);check(audioView==AudioView::Recordings&&lv_dropdown_get_option_count(audioSortSelector)==6,"recording tab exposes six file sort choices");
 check(audioRows->count()==1&&lv_obj_has_state(audioRows->slots_[0].favorite,LV_STATE_USER_1),"recordings retain trash action instead of favorite");
 click(audioRows->slots_[0].favorite);check(deletedPath=="/Recordings/record.wav","actual trash callback targets recording");
 delete audioRows;audioRows=nullptr;refreshAudioList();check(audioRows&&audioRows->count()==1,"actual refresh reconstructs list after memory reclaim");
 const unsigned requests=Tab5Audio.requests;click(audioRadioTab);check(Tab5Audio.requests==requests&&audioFavoritesOnly[1]&&audioRadioCategory=="FR/news","returning to radio retains context without restarting");
 lv_slider_set_value(audioVolumeSlider,0,LV_ANIM_OFF);lv_obj_send_event(audioVolumeSlider,LV_EVENT_VALUE_CHANGED,nullptr);lv_obj_send_event(audioVolumeSlider,LV_EVENT_RELEASED,nullptr);Tab5MediaSession::begin(150);check(Tab5MediaSession::volume()==0,"actual volume slider and release persist mute");
 activePage=Page::Home;Tab5AudioSnapshot background;background.mode=Tab5AudioMode::Radio;background.currentStationUrl="https://example/88";background.nowPlaying="Now playing metadata";audioRememberPlayback(background);
 check(!std::strcmp(Tab5MediaSession::state().selected[1],"https://example/88")&&Tab5MediaSession::state().view==1&&Tab5MediaSession::state().favorites==2,"background playback updates identity while keeping favorite browser context");
 check(!std::strcmp(Tab5MediaSession::state().title[1],"Station 088"),"background radio remembers station name instead of changing song metadata");
 delete audioRows;audioRows=nullptr;lv_obj_delete(pageHost);
 printf("PASS %u checks: actual main Media callbacks/page layout, favorites,600stations,sort,selection,noautoplay,trash,cache recovery and volume release.\n",checks);
}
