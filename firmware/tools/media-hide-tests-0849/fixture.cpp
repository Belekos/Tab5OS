#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "Tab5CatalogueRevision.h"
using String=std::string;using Tab5AudioStation=String;
struct lv_obj_t{bool hidden=false;unsigned children=0;};
constexpr int LV_OBJ_FLAG_HIDDEN=1;
void lv_obj_add_flag(lv_obj_t*p,int){if(p)p->hidden=true;}void lv_obj_remove_flag(lv_obj_t*p,int){if(p)p->hidden=false;}bool lv_obj_is_valid(lv_obj_t*p){return p!=nullptr;}void lv_obj_clean(lv_obj_t*p){p->children=0;}void lv_obj_move_foreground(lv_obj_t*){}void lv_obj_invalidate(lv_obj_t*){}
void lv_label_set_text(lv_obj_t*,const char*){}void lv_label_set_text_static(lv_obj_t*,const char*){}const char*lv_textarea_get_text(lv_obj_t*){return "";}
unsigned removals=0;void lv_obj_remove_event_cb_with_user_data(lv_obj_t*,void(*)(void*),void*){++removals;}
struct Focus{lv_obj_t*focused_=nullptr;void remember(lv_obj_t*){}void clear(){focused_=nullptr;}lv_obj_t*focused(){return focused_;}bool contains(lv_obj_t*p,lv_obj_t*f){return p==f;}}Tab5Focus;
unsigned heapBlocks=0;void* alloc(){++heapBlocks;return malloc(128);}void heap_caps_free(void*p){if(p){assert(heapBlocks);--heapBlocks;free(p);}}
constexpr unsigned MALLOC_CAP_8BIT=1;size_t heap_caps_get_free_size(unsigned){return 1000000-heapBlocks*128;}
struct Totals{size_t psram,internal;};Totals tab5MemoryTotals(){return {heap_caps_get_free_size(0),0};}
class Tab5MediaList{public:lv_obj_t*list_;void*entries_=nullptr,*pending_=nullptr;static void listEvent(void*){}Tab5MediaList(lv_obj_t*p):list_(p),entries_(alloc()),pending_(alloc()){list_->children=25;}~Tab5MediaList();};
// PRODUCTION_DESTRUCTOR
bool storageAvailable=true,stateAvailable=true;unsigned commands=0;
struct Tab5StorageRecovery{explicit operator bool()const{return storageAvailable;}};
unsigned uxQueueMessagesWaiting(void*){return commands;}
enum class Tab5AudioMode{Idle,Local,Radio,Recording};
class Tab5AudioEngine{public:struct StateGuard{StateGuard(Tab5AudioEngine&,unsigned){}explicit operator bool()const{return stateAvailable;}};
Tab5AudioMode _mode=Tab5AudioMode::Idle;bool _priorityCommandPending=false,_libraryLoaded=true,_stationsLoaded=true;void*_commandQueue=(void*)1;std::vector<String>_tracks,_stations;size_t _trackIndex=0,_stationIndex=0;std::atomic<uint32_t>_trackCatalogueRevision{2},_stationCatalogueRevision{2};size_t releaseIdleCatalogues();}Tab5Audio;
// PRODUCTION_CATALOGUES
enum class Page{Home,Files,Editor,Writing,Preview,Audio,Map,ImageEditor,Settings};
enum class HomeAction{Home,Files,Editor,Reader,Audio,Map,ImageEditor,Settings};
Page activePage=Page::Home;HomeAction activeTool=HomeAction::Home;
lv_obj_t objects[16];lv_obj_t *defaultFontPopup=nullptr,*dock=&objects[0],*homePage=&objects[1],*filesPage=&objects[2],*editorPage=&objects[3],*previewPage=&objects[4],*audioPage=&objects[5],*mapPage=&objects[6],*settingsPage=&objects[7],*pageHost=&objects[8],*homeAppsGrid=&objects[9],*previewLabel=&objects[10],*editor=&objects[11],*mapStatusLabel=&objects[12],*audioList=&objects[13];
Tab5MediaList*audioRows=nullptr;std::vector<String>audioAlbumFolders,audioRadioCategories;
bool keyboardDockFocus=false,keyboardUiNavigation=false,sdReady=true,homeAppsScanning=false,homeCustomize=false,homeAppsDirty=false,mapOpenPending=false,mapSearchPending=false,mapSearchResultsReady=false,fileSearchNeedsRestart=false;unsigned homeAppCount=0,mapGeneration=0,mapSearchRequestId=0;
String fileTreeRevealPath,fileListRevealPath;
struct{void cancel(){}}fileTree;struct{void close(){}}fileListDir;struct{bool busy(){return false;}void cancel(){}}fileSearcher;struct{bool done=true;}folderPreview;struct{void cancel(){}}mapWheel;
struct Logger{template<class...T>void printf(const char*,T...){}void println(const char*){}}Tab5DebugSerial;
struct{void suspend(){}}Tab5Tools;
namespace Tab5GitView{void close(){}}namespace Tab5Meshtastic{bool visible(){return false;}void close(){}}
namespace Tab5AppCatalogue{void endScan(){}void loadChoices(){}}namespace Tab5ImageEditor{void show(bool){}lv_obj_t*scope(){return &objects[14];}}
struct{bool ready(){return true;}void setActive(bool,unsigned){}}Tab5Map;
struct{void show(bool){}lv_obj_t*object(){return &objects[15];}}Tab5Writing;
unsigned videoTrims=0;struct{size_t reclaimIdle(){assert(audioPage->hidden);++videoTrims;return 0;}}Tab5Video;
unsigned remembered=0,flushed=0;
void closeDefaultFontPopup(){}void rememberFileSession(){}lv_obj_t*keyboardScope(){return pageHost;}
void toast(const char*,unsigned){}void audioRememberScroll(){++remembered;}void audioRememberBrowser(){++remembered;}namespace Tab5MediaSession{void flush(unsigned){++flushed;}}unsigned millis(){return 1;}
void cancelFolderPreview(){}void freeFilePreviewImage(){}void saveMapLeavingView(){}void freeMapCanvas(){}void updateResourceForeground(){}void setKeyboardVisible(bool){}void ensureFileSession(){}void updateEditorStatus(bool){}void updateAudioUi(bool){}void restoreMapOpeningView(){}void refreshSystemInfo(){}void updateTopBarForPage(){}void updateNavState(){}const char*T(const char*s){return s;}void setLabelIfChanged(lv_obj_t*,const char*){}
void refreshAudioList(){assert(activePage==Page::Audio);if(!audioRows)audioRows=new Tab5MediaList(audioList);if(!Tab5Audio._libraryLoaded){Tab5Audio._tracks={"track"};Tab5Audio._libraryLoaded=true;}if(!Tab5Audio._stationsLoaded){Tab5Audio._stations={"radio"};Tab5Audio._stationsLoaded=true;}}
// PRODUCTION_TRANSITION
int main(){
 unsigned checks=0;
 for(auto mode:{Tab5AudioMode::Idle,Tab5AudioMode::Local,Tab5AudioMode::Radio,Tab5AudioMode::Recording})for(unsigned cycle=0;cycle<25;++cycle){
  Tab5Audio._mode=mode;Tab5Audio._tracks={"original track"};Tab5Audio._stations={"original station"};Tab5Audio._libraryLoaded=Tab5Audio._stationsLoaded=true;
  showPage(Page::Audio);assert(audioRows&&heapBlocks==2&&!audioPage->hidden);++checks;
  audioAlbumFolders={"album"};audioRadioCategories={"category"};Tab5Focus.focused_=audioList;
  const auto scroll=remembered;showPage(Page::Home);
  assert(audioPage->hidden&&!audioRows&&heapBlocks==0&&audioList->children==0);assert(audioAlbumFolders.empty()&&audioRadioCategories.empty());assert(remembered==scroll+2);checks+=3;
  assert(Tab5Audio._tracks.empty()==(mode!=Tab5AudioMode::Local&&mode!=Tab5AudioMode::Recording));assert(Tab5Audio._stations.empty()==(mode!=Tab5AudioMode::Radio&&mode!=Tab5AudioMode::Recording));assert(Tab5Audio._mode==mode);checks+=3;
 }
 for(unsigned veto=0;veto<4;++veto){
  Tab5Audio._mode=Tab5AudioMode::Idle;Tab5Audio._tracks={"keep"};Tab5Audio._stations={"keep"};Tab5Audio._libraryLoaded=Tab5Audio._stationsLoaded=true;showPage(Page::Audio);
  commands=veto==0;storageAvailable=veto!=1;stateAvailable=veto!=2;Tab5Audio._priorityCommandPending=veto==3;
  showPage(Page::Home);assert(!audioRows&&Tab5Audio._tracks.size()==1&&Tab5Audio._stations.size()==1);++checks;
  commands=0;storageAvailable=stateAvailable=true;Tab5Audio._priorityCommandPending=false;
 }
 assert(videoTrims==104&&removals==104&&heapBlocks==0);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"pageExitCycles\":104,\"outstandingExplicitBuffers\":"<<heapBlocks<<"}"<<std::endl;
}
