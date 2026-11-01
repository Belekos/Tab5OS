#include "Tab5ResourceManager.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using namespace Tab5Resources;
static unsigned checks=0;
static void check(bool yes,const char* message){++checks;if(!yes){fprintf(stderr,"FAIL %s\n",message);std::abort();}}
static size_t freeInternal=256*KiB,freePsram=550*KiB;
static MemorySample sample(){return {freeInternal,freeInternal,freeInternal,freePsram,freePsram};}
static MemoryTotals totals(){return {freeInternal,freePsram};}
static MemoryTotals tab5MemoryTotals(){return totals();}
static Manager* currentManager=nullptr;
#define Tab5MemoryManager (*currentManager)
enum class Page {Home,Files,Editor,Writing,Preview,Audio,Map,Settings,ImageEditor};
enum class AudioView {Videos,Music};
static Page activePage=Page::Home;static AudioView audioView=AudioView::Music;
static bool videoViewerActive=false,readerActive=false,mapJpegPreparing=false,mapJpegEncoding=false;
struct Tools {bool shown=false;void* scope(){return shown?this:nullptr;}} Tab5Tools;
struct Writing {unsigned calls=0;size_t reclaimHiddenLayout(){++calls;return 0;}} Tab5Writing;
struct Video {unsigned calls=0;size_t reclaimIdle(){++calls;return 0;}} Tab5Video;
struct Audio {unsigned catalogues=0,worker=0;size_t releaseIdleCatalogues(){++catalogues;return 0;}size_t reclaimIdleWorker(){++worker;return 0;}} Tab5Audio;
struct Search {unsigned cancels=0;void cancel(){++cancels;}} fileSearcher;
struct Directory {unsigned closes=0;void close(){++closes;}} fileListDir;
struct Object{};static Object list,row;
static Object* fileList=nullptr;static Object* selectedFileBtn=nullptr;static Object* audioList=nullptr;
static unsigned previewCancels=0,listClears=0;
static void cancelFolderPreview(){++previewCancels;}
static void lv_obj_clean(Object* object){check(object==fileList,"only registered file list cleaned");check(selectedFileBtn==nullptr,"selection invalidated before child deletion");++listClears;freePsram+=MiB;}
static unsigned deletedRows=0;
struct Rows{~Rows(){++deletedRows;freePsram+=MiB;}};static Rows* audioRows=nullptr;
using String=std::string;static std::vector<String> audioAlbumFolders,audioRadioCategories;
namespace Tab5MapPrint {static bool printing=false;static bool busy(){return printing;}}
struct Map {unsigned clears=0;size_t bytes=MiB;size_t memoryCacheBytes(){return bytes;}void clearMemoryCache(){++clears;freePsram+=bytes;bytes=0;}} Tab5Map;
#include "resource_integration.inc"
static void reset(){freeInternal=256*KiB;freePsram=550*KiB;Tab5Tools.shown=false;videoViewerActive=readerActive=mapJpegPreparing=mapJpegEncoding=false;activePage=Page::Home;audioView=AudioView::Music;
 Tab5Writing={};Tab5Video={};Tab5Audio={};fileSearcher={};fileListDir={};Tab5Map={};Tab5MapPrint::printing=false;
 fileList=selectedFileBtn=audioList=nullptr;delete audioRows;audioRows=nullptr;audioAlbumFolders.clear();audioRadioCategories.clear();previewCancels=listClears=deletedRows=0;freePsram=550*KiB;}
static size_t probe(void* ctx){++*static_cast<unsigned*>(ctx);return 0;}
static void ownership(){
 struct Scenario{Page page;bool tools,video,reader;Owner owner;};
 const Scenario cases[]={
 {Page::Home,false,false,false,Owner::System},{Page::Files,false,false,false,Owner::Files},
 {Page::ImageEditor,false,false,false,Owner::Image},{Page::Map,false,false,false,Owner::Map},
 {Page::Audio,false,false,false,Owner::Audio},{Page::Editor,false,false,false,Owner::Writing},
 {Page::Writing,false,false,false,Owner::Writing},{Page::Settings,false,false,false,Owner::System},
 {Page::ImageEditor,true,false,false,Owner::Tools},{Page::Files,false,true,false,Owner::Video},
 {Page::Files,false,false,true,Owner::Reader}};
 for(const auto& item:cases){
   reset();Manager manager(sample,totals);currentManager=&manager;unsigned protectedCalls=0,otherCalls=0;
   const Owner other=item.owner==Owner::Map?Owner::Files:Owner::Map;
   manager.addCache(item.owner,"current",probe,&protectedCalls,{MemoryDomain::Psram});
   manager.addCache(other,"background",probe,&otherCalls,{MemoryDomain::Psram});
   activePage=item.page;Tab5Tools.shown=item.tools;videoViewerActive=item.video;readerActive=item.reader;
   updateResourceForeground();manager.collect({0,0,0,128*KiB,0},item.owner==Owner::System?Owner::Tools:Owner::System);
   check(protectedCalls==0,"actual foreground routing protects visible owner");
   check(otherCalls==1,"routing does not mark every cache protected");
 }
}
static void callbacks(){
 for(unsigned scenario=0;scenario<5;++scenario){
   reset();Manager manager(sample,totals);currentManager=&manager;registerResourceCaches();
   Tab5Tools.shown=true;updateResourceForeground(); // overlay owns focus; underlying cache still needs a veto
   if(scenario==0)activePage=Page::Map;
   if(scenario==1)Tab5MapPrint::printing=true;
   if(scenario==2)mapJpegPreparing=true;
   if(scenario==3)mapJpegEncoding=true;
   manager.collect({0,0,0,128*KiB,0});
   check(Tab5Map.clears==(scenario==4?1u:0u),"map callback protects page/print/JPEG, frees only hidden unused tiles");
 }
 for(bool visible:{false,true}){
   reset();Manager manager(sample,totals);currentManager=&manager;registerResourceCaches();
   fileList=&list;selectedFileBtn=&row;activePage=visible?Page::Files:Page::Home;
   Tab5Tools.shown=true;updateResourceForeground();manager.collect({0,0,0,128*KiB,0});
   check(listClears==(visible?0u:1u),"Explorer view under overlay never cleaned");
   check(fileSearcher.cancels==(visible?0u:1u)&&fileListDir.closes==(visible?0u:1u)&&previewCancels==(visible?0u:1u),"hidden Explorer release stops owners before deleting rows");
 }
 for(bool visible:{false,true}){
   reset();Manager manager(sample,totals);currentManager=&manager;registerResourceCaches();
   audioList=&list;audioRows=new Rows;audioAlbumFolders={"album"};audioRadioCategories={"radio"};
   activePage=visible?Page::Audio:Page::Home;audioView=AudioView::Videos;
   Tab5Tools.shown=true;updateResourceForeground();manager.collect({0,0,0,128*KiB,0});
   check(Tab5Audio.catalogues==(visible?0u:1u),"media catalogue not released beneath visible audio page");
   check(deletedRows==(visible?0u:1u),"media rows released only when page hidden");
   check(Tab5Video.calls==(visible?0u:1u),"visible video library protected beneath overlay");
   if(!visible)check(audioRows==nullptr&&audioAlbumFolders.empty()&&audioRadioCategories.empty(),"hidden media callback clears owning pointers");
 }
 reset();
}
int main(){ownership();callbacks();printf("{\"passed\":true,\"checks\":%u,\"foregroundScenarios\":11,\"callbackScenarios\":9}\n",checks);}
