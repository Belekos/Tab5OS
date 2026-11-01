// Executes the extracted production serviceHomeApps with observable I/O boundaries.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Tab5AppRefresh.h"
static unsigned checks=0;
static void check(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
enum class Page { Home, Other };
static Page activePage=Page::Home;
static int grid,home,sidebar,icon;
static void* homeAppsGrid=&grid;
static void* homePage=&home;
static void* currentScope=&home;
static void* sidebarSettingsOverlay=nullptr;
static void* homeIconBoxes[64]{};
static bool homeAppsScanning=false,homeAppsDirty=true,homeCustomize=false,homeRenderPending=false,sdReady=true;
static unsigned homeMenu=0,homeIconNext=0;
static uint32_t homePublishedRevision=0,clockMs=1,fsRevision=0;
static unsigned begins=0,ends=0,steps=0,publications=0,renders=0,icons=0,toasts=0;
static unsigned finishAfter=2,remaining=0;
static bool needsExternal=false,toolsOpen=false,usbMounted=false,publishOnFinish=false;
static uint32_t usbRevision=0,catalogueRevision=0;
static uint32_t millis(){return clockMs;}
static uint32_t tab5FsRevision(){return fsRevision;}
static void* keyboardScope(){return currentScope;}
struct Tools {void* scope(){return toolsOpen?&grid:nullptr;}} Tab5Tools;
namespace Tab5UsbStorage {static uint32_t revision(){return usbRevision;}static bool mounted(){return usbMounted;}}
namespace Tab5Shell {struct Choices{bool enabled=true;}choices;}
namespace Tab5AppCatalogue {
 static bool beginScan(bool mounted){++begins;remaining=finishAfter;if(!mounted&&!usbMounted){++catalogueRevision;return false;}return true;}
 static void endScan(){++ends;remaining=0;}
 static int scanStep(){++steps;if(remaining&&--remaining)return 0;if(publishOnFinish)++catalogueRevision;return 1;}
 static uint32_t revision(){return catalogueRevision;}
 static unsigned count(){return 0;}
 static bool attachIcon(void*,unsigned){return true;}
}
static bool shellDockNeedsCatalogue(){return needsExternal;}
static void shellServiceDockIcon(){++icons;}
static void shellCatalogueChanged(){++publications;}
static void homeRequestRender(){homeRenderPending=true;}
static void renderHomeApps(){++renders;homeRenderPending=false;}
static void toast(const char*,unsigned){++toasts;}
#include "service.inc"
static void tick(unsigned milliseconds=100){clockMs+=milliseconds;serviceHomeApps();}
static void cacheTraffic(unsigned iterations){for(unsigned i=0;i<iterations;++i){++fsRevision;tick();}}
int main(int argc,char** argv){
 check(argc==2,"scenario specified");
 if(!std::strcmp(argv[1],"native-idle")){
  serviceHomeApps();cacheTraffic(1200);
  check(begins==0&&steps==0&&ends==0,"native home does not scan under two minutes of cache mutations");
  check(publications==0&&renders==0,"idle cache mutations never invalidate home/dock");
 }else if(!std::strcmp(argv[1],"external-idle")){
  needsExternal=true;publishOnFinish=true;serviceHomeApps();tick();
  check(begins==1&&!homeAppsScanning,"external dock scans once on startup");
  check(publications==1&&renders==1,"one changed publication refreshes once");
  cacheTraffic(1200);
  check(begins==1&&steps==2&&publications==1&&renders==1,"external or missing shortcut does not rescan on cache traffic");
 }else if(!std::strcmp(argv[1],"support")){
  needsExternal=true;serviceHomeApps();tick();
  sdReady=false;tick();
  check(begins==2&&!homeAppsScanning,"SD removal requests one complete no-support publication");
  cacheTraffic(100);check(begins==2,"missing support never produces a retry loop");
  sdReady=true;tick();tick();check(begins==3&&!homeAppsScanning,"SD reconnect requests one new scan");
  usbMounted=true;++usbRevision;tick();tick();check(begins==4,"USB support revision requests one scan");
  cacheTraffic(100);check(begins==4,"stable support remains idle");
 }else if(!std::strcmp(argv[1],"catalogue")){
  homeMenu=1;serviceHomeApps();tick();check(begins==1&&!homeAppsScanning,"visible catalogue initial scan");
  ++fsRevision;tick();for(unsigned i=0;i<15;++i)tick();check(begins==1,"mutation obeys minimum scan interval");
  for(unsigned i=0;i<4;++i)tick();check(begins==2&&!homeAppsScanning,"debounced mutation eventually rescans catalogue");
  for(unsigned i=0;i<52;++i)tick();check(begins==3&&!homeAppsScanning,"visible catalogue polls external file writers");
  homeMenu=0;cacheTraffic(100);check(begins==3,"closing catalogue stops periodic refresh");
  check(publications==0&&renders==0,"identical catalogue never refreshes widgets");
 }else if(!std::strcmp(argv[1],"cancel")){
  needsExternal=true;finishAfter=100;serviceHomeApps();check(homeAppsScanning,"scan in flight");
  activePage=Page::Other;tick();check(ends==1&&!homeAppsScanning&&homeAppsDirty,"leaving home cancels once and retains pending discovery");
  cacheTraffic(100);check(begins==1&&steps==1&&ends==1,"no catalogue I/O on another app");
  activePage=Page::Home;tick();check(begins==2&&homeAppsScanning,"returning home resumes cancelled discovery");
  toolsOpen=true;tick();check(ends==2&&!homeAppsScanning,"Tools takes priority over catalogue worker");
  cacheTraffic(100);check(begins==2&&steps==2,"Tools does not trigger repeated scans");
  toolsOpen=false;finishAfter=1;tick();check(begins==3&&!homeAppsScanning,"discovery resumes once after Tools closes");
 }else if(!std::strcmp(argv[1],"settings")){
  needsExternal=true;Tab5Shell::choices.enabled=false;serviceHomeApps();cacheTraffic(100);
  check(begins==0,"disabled dock never scans for saved external entries");
  activePage=Page::Other;currentScope=nullptr;sidebarSettingsOverlay=&sidebar;tick();tick();
  check(begins==1&&!homeAppsScanning,"sidebar settings discovers apps outside home");
  sidebarSettingsOverlay=nullptr;cacheTraffic(100);check(begins==1,"closing settings stops scans outside home");
  activePage=Page::Home;currentScope=&home;Tab5Shell::choices.enabled=true;homeAppsDirty=true;tick();tick();
  check(begins==2,"reenabling and explicit refresh discovers saved shortcuts");
 }else check(false,"unknown scenario");
 std::printf("{\"passed\":true,\"scenario\":\"%s\",\"checks\":%u,\"scansStarted\":%u,\"scanSteps\":%u,\"scansCancelled\":%u,\"publications\":%u,\"renders\":%u}\n",argv[1],checks,begins,steps,ends,publications,renders);
}
