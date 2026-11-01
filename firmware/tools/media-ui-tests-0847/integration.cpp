#include "Platform.h"
#include <lvgl.h>
#include "Tab5Search.h"
#include "Tab5TextInput.cpp"
#include "Tab5UiStyle.cpp"
#define private public
#include "Tab5MediaList.h"
#undef private
#include "Tab5MediaList.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks=0,toasts=0,libraryLoads=0;
static void check(bool value,const char* what){if(!value){fprintf(stderr,"FAIL %s\n",what);exit(1);}++checks;}
enum class AudioView:unsigned {Local,Radio,Recordings,Videos};
enum class Page {Audio,Home};static Page activePage=Page::Audio;
static AudioView audioView=AudioView::Local;
static String audioSearchQueries[4],audioAlbumFolder,audioRadioCategory;
static bool audioFavoritesOnly[2]{},audioListRebuilding=false;
static AudioView audioRenderedView=AudioView::Local;static uint32_t audioRenderedRevision=0;static bool audioRefreshPending=false,videoViewerActive=false;static lv_obj_t* audioPage=nullptr;
static unsigned posted=0,deleted=0,favorited=0,remembered=0;static String deletedPath,favoritePath;
static std::vector<String> audioAlbumFolders,audioRadioCategories;
static lv_obj_t *audioList,*audioListTitle,*audioSortSelector,*audioFavoritesTab,*audioFavoritesSelector,*audioNewFolder,*audioSearchClear;
static lv_obj_t *audioLocalTab,*audioRadioTab,*audioRecordTab,*audioVideoTab;
static Tab5MediaList* audioRows=nullptr;
struct Tab5AudioStation{String name,url,style;};struct Tab5VideoItem{String name,source;bool remote=false;float fps=25.0f;String format;};
struct AudioBoundary {
 uint32_t tracksRevision=2,stationsRevision=2;bool changeDuringRead=false;
 uint32_t trackCatalogueRevision(){return tracksRevision;}uint32_t stationCatalogueRevision(){return stationsRevision;}
 bool playTrack(size_t,uint32_t rev=0){if(rev&&rev!=tracksRevision)return false;++posted;return true;}bool playStation(size_t,uint32_t rev=0){if(rev&&rev!=stationsRevision)return false;++posted;return true;}
 std::vector<std::pair<String,String>> tracks;std::vector<Tab5AudioStation> stations;
 void ensureLibraryLoaded(){++libraryLoads;}size_t stationCount(){return stations.size();}size_t trackCount(){return tracks.size();}
 bool station(size_t i,Tab5AudioStation& v){if(i>=stations.size())return false;v=stations[i];if(changeDuringRead){changeDuringRead=false;stationsRevision+=2;}return true;}
 String trackPath(size_t i){return i<tracks.size()?tracks[i].first:String();}String trackName(size_t i){return i<tracks.size()?tracks[i].second:String();}
}Tab5Audio;
struct VideoBoundary {uint32_t revision=2;unsigned reads=0,changeAt=0;uint32_t catalogueRevision(){return revision;}void stop(){}std::vector<Tab5VideoItem> items;void ensureLibraryLoaded(){++libraryLoads;}size_t count(){return items.size();}bool item(size_t i,Tab5VideoItem& v){if(i>=items.size())return false;v=items[i];if(++reads==changeAt){revision+=2;changeAt=0;}return true;}}Tab5Video;
namespace Tab5Favorites {bool contains(bool,const String&){return false;}bool toggle(bool,const String& path){++favorited;favoritePath=path;return true;}}
namespace Tab5MediaSession {
struct Snapshot{unsigned view=0;uint8_t favorites=0;char folder[256]{},category[256]{},search[4][256]{},selected[4][256]{},title[4][256]{};int32_t scroll[4]{};}saved;
const Snapshot& state(){return saved;}
void browser(unsigned view,uint8_t favorites,const char* folder,const char* category,uint32_t){saved.view=view;saved.favorites=favorites;snprintf(saved.folder,sizeof(saved.folder),"%s",folder);snprintf(saved.category,sizeof(saved.category),"%s",category);}
void search(unsigned view,const char* query,uint32_t){snprintf(saved.search[view],sizeof(saved.search[view]),"%s",query);}
void scroll(unsigned view,int32_t value,uint32_t){saved.scroll[view]=value;}
void select(unsigned view,const char* id,const char* title,uint32_t){++remembered;snprintf(saved.selected[view],sizeof(saved.selected[view]),"%s",id);snprintf(saved.title[view],sizeof(saved.title[view]),"%s",title);}
}
static const char* T(const char* text){return text;}static void toast(const char*){++toasts;}
static const lv_font_t* uiFont(){return LV_FONT_DEFAULT;}
static String baseNameOf(const String& path){int p=path.lastIndexOf('/');return p<0?path:path.substring(p+1);}
static String parentDirOf(const String& path){int p=path.lastIndexOf('/');return p<=0?String("/"):path.substring(0,p);}
static String joinPath(const String& a,const String& b){return a+"/"+b;}
static void setLabelIfChanged(lv_obj_t* label,const char* text){if(strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);}
static String audioAlbumName(const String& path){return path;}
static void audioMediaFormat(Tab5MediaList::Entry,String&,const char*&,bool&,bool&,bool&);
static void audioMediaAction(Tab5MediaList::Entry,bool);
static void refreshAudioList();
static void updateAudioUi(bool);
static void showPage(Page page){activePage=page;}
static void showPythonLibrary(bool){}
static void startVideoViewerSource(const String&,const String&,float,const String&){++posted;}
static void askDeletePath(const String& path){++deleted;deletedPath=path;}
static lv_obj_t* makeIconButton(lv_obj_t* p,const char*,int w,int h){auto* o=lv_button_create(p);lv_obj_set_size(o,w,h);return o;}
static lv_obj_t* makeMicrophoneButton(lv_obj_t* p,int w,int h,void*){return makeIconButton(p,"",w,h);}
#include "integration-extracted.inc"

static void click(lv_obj_t* o){lv_obj_send_event(o,LV_EVENT_PRESSED,nullptr);lv_obj_send_event(o,LV_EVENT_RELEASED,nullptr);lv_obj_send_event(o,LV_EVENT_CLICKED,nullptr);}
static bool tabsMatch(){lv_obj_t* tabs[]={audioLocalTab,audioRadioTab,audioRecordTab,audioVideoTab};for(unsigned i=0;i<4;++i)if(lv_obj_has_state(tabs[i],LV_STATE_CHECKED)!=(i==unsigned(audioView)))return false;return true;}
int main(){
 lv_init();auto* display=lv_display_create(1280,720);auto* screen=lv_screen_active();audioPage=screen;Tab5Focus.begin(screen,LV_FONT_DEFAULT);
 audioList=lv_obj_create(screen);lv_obj_set_size(audioList,540,500);audioListTitle=lv_label_create(screen);
 audioSortSelector=lv_dropdown_create(screen);audioFavoritesSelector=lv_dropdown_create(screen);lv_dropdown_set_options(audioFavoritesSelector,"All\nFavorites");
 audioFavoritesTab=lv_button_create(screen);audioNewFolder=lv_button_create(screen);audioSearchClear=lv_button_create(screen);
 auto* source=lv_obj_create(screen);createExtractedTabs(source);
 Fake::folders["/Audio"]={};Fake::folders["/mp3"]={};
 for(unsigned i=0;i<100;++i){char path[64],name[40];snprintf(path,sizeof(path),"/Audio/%03u.mp3",i);snprintf(name,sizeof(name),"Track %03u",i);Tab5Audio.tracks.push_back({path,name});Fake::metadata[path]={i,100-i};}
 for(unsigned i=0;i<120;++i){char name[40],url[64];snprintf(name,sizeof(name),"Station %03u",i);snprintf(url,sizeof(url),"http://fixture/%u",i);Tab5Audio.stations.push_back({name,url,"News"});}
 Tab5Video.items.push_back({"Video","/Videos/fixture.mjpeg",false});
 audioAlbumFolder="/Audio";refreshAudioList();check(audioRows&&audioRows->count()==101,"initial folder includes parent and all tracks");check(tabsMatch(),"initial exclusive tabs");
 audioView=AudioView::Videos;refreshAudioList();
 unsigned repeatMismatch=0;
 lv_obj_t* tabs[]={audioLocalTab,audioRadioTab,audioRecordTab,audioVideoTab};
 for(unsigned cycle=0;cycle<20;++cycle){for(unsigned i=0;i<4;++i){click(tabs[i]);check(unsigned(audioView)==i&&tabsMatch(),"different tab activates its view exclusively");click(tabs[i]);if(!tabsMatch())++repeatMismatch;}}
#if CURRENT
 check(repeatMismatch==0,"repeated active-tab clicks retain exclusive checked state");
#endif
 audioView=AudioView::Radio;audioRadioCategory="News";refreshAudioList();
 const auto count=audioRows->count();std::vector<Tab5MediaList::Entry> old(audioRows->entries_,audioRows->entries_+count);
 const auto title=std::string(lv_label_get_text(audioListTitle));
 audioView=AudioView::Local;audioAlbumFolder="/Audio";HeapCheck::failAfter=0;refreshAudioList();HeapCheck::failAfter=-1;
 const bool rolledBack=audioView==AudioView::Radio&&audioRadioCategory=="News"&&audioRows->count()==count;
#if CURRENT
 check(rolledBack,"failed replacement restores browser and complete catalogue");check(title==lv_label_get_text(audioListTitle),"failed replacement keeps matching heading");check(tabsMatch(),"failed replacement keeps matching selected tab");
 for(size_t i=0;i<count;++i)check(old[i].kind==audioRows->entries_[i].kind&&old[i].index==audioRows->entries_[i].index,"failed replacement keeps every displayed action reference");
#endif
 check(toasts==1,"allocation failure is visible once");
 // Retry must work after cancellation, including changing the type of catalogue.
 audioView=AudioView::Local;audioAlbumFolder="/Audio";refreshAudioList();check(audioView==AudioView::Local&&audioRows->count()==101&&tabsMatch(),"successful retry replaces failed request");
#if CURRENT
 // A source reload followed by OOM cannot retain actionable numeric indices.
 audioView=AudioView::Recordings;Tab5Audio.tracks.push_back({"/Recordings/A.wav","A"});Tab5Audio.tracks.push_back({"/Recordings/B.wav","B"});Tab5Audio.tracksRevision+=2;refreshAudioList();
 const auto savedDeletes=deleted;const auto savedPosted=posted;
 std::swap(Tab5Audio.tracks[100],Tab5Audio.tracks[101]);Tab5Audio.tracksRevision+=2;
 HeapCheck::failAfter=0;audioMediaAction({Tab5MediaList::Media,100},true);HeapCheck::failAfter=-1;
 check(deleted==savedDeletes&&posted==savedPosted,"stale recording action cannot delete or play a different file");
 check(audioRows->count()==0,"reload plus OOM discards invalid references");
 const auto loadsAfterFailure=libraryLoads;for(unsigned i=0;i<100;++i)updateAudioUi(false);
 check(libraryLoads==loadsAfterFailure,"same failed generation is not retried every UI loop");
 refreshAudioList();audioMediaAction({Tab5MediaList::Media,100},true);
 check(deleted==savedDeletes+1&&deletedPath=="/Recordings/B.wav","new explicit selection resolves refreshed recording identity");
 audioView=AudioView::Radio;audioRadioCategory="News";refreshAudioList();
 const auto oldRevision=audioRenderedRevision;Tab5Audio.changeDuringRead=true;refreshAudioList();
 check(audioRefreshPending&&audioRows->count()==0,"source changing during model build is not published");
 updateAudioUi(false);check(!audioRefreshPending&&audioRenderedRevision!=oldRevision&&audioRows->count()==121,"completed source revision is rebuilt once");
 const auto selectedBefore=remembered;const auto playBefore=posted;Tab5Audio.changeDuringRead=true;audioMediaAction({Tab5MediaList::Media,0},false);
 check(remembered==selectedBefore&&posted==playBefore,"source changing during selection never persists or queues mismatched identity");
 refreshAudioList();audioMediaAction({Tab5MediaList::Media,0},false);check(posted==playBefore+1,"fresh selection still starts playback");
 audioView=AudioView::Videos;refreshAudioList();const auto videoBefore=posted;Tab5Video.reads=0;Tab5Video.changeAt=2;
 audioMediaAction({Tab5MediaList::Media,0},false);check(posted==videoBefore,"video changed between remembered identity and second copy never launches a different item");
 refreshAudioList();audioMediaAction({Tab5MediaList::Media,0},false);check(posted==videoBefore+1,"fresh video selection launches copied source");
#endif
 delete audioRows;audioRows=nullptr;check(HeapCheck::live==0,"integration releases both models and sort arena");lv_display_delete(display);
 printf("{\"checks\":%u,\"sameTabStateFailures\":%u,\"failedReplacementRolledBack\":%s,\"trackedLiveBytes\":%zu,\"libraryBoundaries\":%u}\n",checks,repeatMismatch,rolledBack?"true":"false",HeapCheck::live,libraryLoads);
}
