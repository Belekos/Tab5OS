#include "Tab5AppCatalogue.h"
#include "Tab5Ico.h"
#include "Tab5PythonServices.h"
#include "Tab5Storage.h"
#include "Tab5VolumeFS.h"
#include "Tab5UsbStorage.h"
#include "Tab5Task.h"
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <cerrno>
#include <cstdio>
#include <new>
#include <utility>
#include <strings.h>
#include <misc/cache/instance/lv_image_cache.h>
#include <misc/cache/instance/lv_image_header_cache.h>
namespace Tab5AppCatalogue {
static int stepScanIo();
namespace {
App *apps=nullptr,*staging=nullptr;
unsigned used=0,staged=0,entries=0,rootIndex=0;
bool scanActive=false;
std::atomic<bool> workerRunning{false},cancelScan{false};
std::atomic<int> workerResult{0};
bool workerMode=false,queuedScan=false,queuedSd=false;
constexpr int Cancelled=-2;
constexpr uint32_t ScanStack=8192;
uint32_t catalogueRevision=0;
int phase=0;
bool truncated=false,failed=false;
File directory;
bool scanSd=false;
Tab5StorageUse* lease=nullptr;
Choices config;
constexpr const char* roots[]={"/Applications","/Apps","/MicroPython","/USB/Applications","/USB/Apps","/USB/MicroPython"};
size_t iconBytes=0;
constexpr size_t MaxIconBytes=192*1024;
struct OwnedIcon {lv_image_dsc_t image{};uint8_t* pixels=nullptr;size_t size=0;};
bool regular(const char* logical){
  errno=0;File file=tab5FS.open(logical,FILE_READ);
  if(!file){if(errno!=ENOENT)failed=true;return false;}
  return !file.isDirectory();
}
bool add(const App& app,bool stagedEntry=false){
  App* target=stagedEntry?staging:apps;unsigned& count=stagedEntry?staged:used;
  if(!target)return false;
  for(unsigned i=0;i<count;++i)if(target[i].key==app.key)return false;
  if(count>=MaxApps){if(stagedEntry)truncated=true;return false;}
  target[count++]=app;return true;
}
void titleFor(App& app,const char* leaf){
  size_t n=strlen(leaf);
  if(n>8&&!strcasecmp(leaf+n-8,".app.elf"))n-=8;
  else if(n>4&&!strcasecmp(leaf+n-4,".elf"))n-=4;
  else if(n>3&&!strcasecmp(leaf+n-3,".py"))n-=3;
  if(n>=sizeof(app.title))n=sizeof(app.title)-1;
  while(n&&(uint8_t(leaf[n])&0xc0)==0x80)--n;
  memcpy(app.title,leaf,n);app.title[n]=0;
  for(char* p=app.title;*p;++p)if(*p=='_')*p=' ';
}
// Keep the published table stable while directories are read incrementally.
// File enumeration order alone must not replace indices used by visible cards.
bool equivalent(const App& a,const App& b){
  return a.key==b.key&&a.kind==b.kind&&a.action==b.action&&a.symbol==b.symbol
      &&!strcmp(a.id,b.id)&&!strcmp(a.title,b.title);
}
bool changedCatalogue(){
  if(staged!=used)return true;
  for(unsigned i=0;i<staged;++i){
    unsigned j=0;while(j<used&&apps[j].key!=staging[i].key)++j;
    if(j==used||!equivalent(apps[j],staging[i]))return true;
  }
  return false;
}
int terminalResult(){return failed?-1:truncated?2:1;}
int publishScan(int result){
  if(result>0&&scanActive&&changedCatalogue()){
    std::swap(apps,staging);used=staged;++catalogueRevision;
  }
  scanActive=false;staged=0;workerMode=false;
  return result==Cancelled?-1:result;
}
void closeScanIo(){
  Tab5PythonServices::endAppScan();directory.close();
  delete lease;lease=nullptr;phase=0;
}
bool startScanIo(){
  lease=new(std::nothrow) Tab5StorageUse(true);
  if(!lease||!bool(*lease)){failed=true;return false;}
  phase=scanSd&&Tab5PythonServices::beginAppScan()?1:2;
  return true;
}
void scanWorker(void*){
  int result=-1;
  if(!cancelScan.load(std::memory_order_acquire)&&startScanIo()){
    auto yieldedAt=xTaskGetTickCount();
    do {
      if(cancelScan.load(std::memory_order_acquire)){result=Cancelled;break;}
      result=stepScanIo();
      // Cached directory walks still have to let the idle watchdog run.
      if(result==0&&xTaskGetTickCount()-yieldedAt>=pdMS_TO_TICKS(8)){
        vTaskDelay(1);yieldedAt=xTaskGetTickCount();
      }
    }while(result==0);
  }
  closeScanIo();
  if(cancelScan.load(std::memory_order_acquire))result=Cancelled;
  tab5TaskCheckpoint("catalogue");
  workerResult.store(result,std::memory_order_relaxed);
  // No catalogue or filesystem access after this release publication.
  workerRunning.store(false,std::memory_order_release);
}
unsigned rank(unsigned index){int i=config.find(apps[index].key);return i>=0?config.apps[i].order:MaxApps+index;}
bool before(unsigned a,unsigned b){
  const App& left=apps[a];const App& right=apps[b];
  if(left.kind!=right.kind){auto group=[](Kind kind){return kind==Kind::Builtin?0:kind==Kind::Executable?1:2;};return group(left.kind)<group(right.kind);}
  if(config.personal){unsigned ra=rank(a),rb=rank(b);if(ra!=rb&&(ra<MaxApps||rb<MaxApps))return ra<rb;}
  if(left.kind==Kind::Builtin)return a<b;
  const int title=strcasecmp(left.title,right.title);return title?title<0:strcasecmp(left.id,right.id)<0;
}
OwnedIcon* readIcon(const char* path){
  File file=tab5FS.open(path,FILE_READ);if(!file||file.isDirectory())return nullptr;
  size_t bytes=file.size();if(bytes<22||bytes>32768)return nullptr;
  auto* data=static_cast<uint8_t*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!data)return nullptr;
  const bool read=file.read(data,bytes)==bytes;file.close();Tab5Ico::Frame frame;
  if(!read||!Tab5Ico::inspect(data,bytes,frame)){heap_caps_free(data);return nullptr;}
  size_t amount=size_t(frame.w)*frame.h*4;
  if(amount>MaxIconBytes-iconBytes){heap_caps_free(data);return nullptr;}
  auto* owned=new(std::nothrow) OwnedIcon;
  if(!owned){heap_caps_free(data);return nullptr;}
  owned->pixels=static_cast<uint8_t*>(heap_caps_malloc(amount,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!owned->pixels||!Tab5Ico::decode(frame,owned->pixels,amount)){heap_caps_free(data);heap_caps_free(owned->pixels);delete owned;return nullptr;}
  owned->size=amount;iconBytes+=amount;
  owned->image.header.magic=LV_IMAGE_HEADER_MAGIC;
  owned->image.header.cf=LV_COLOR_FORMAT_ARGB8888;
  owned->image.header.w=frame.w;owned->image.header.h=frame.h;
  owned->image.header.stride=frame.w*4;
  owned->image.data_size=amount;owned->image.data=owned->pixels;
  heap_caps_free(data);return owned;
}
void freeIcon(lv_event_t* event){
  auto* owned=static_cast<OwnedIcon*>(lv_event_get_user_data(event));
  lv_image_cache_drop(&owned->image);lv_image_header_cache_drop(&owned->image);iconBytes-=owned->size;
  heap_caps_free(owned->pixels);delete owned;
}
}
Choices& choices(){return config;}
bool loadChoices(){
  config=Choices{};Preferences prefs;if(!prefs.begin("tab5-home",true))return true;
  Choices candidate;const bool found=prefs.getBytesLength("choices")==sizeof(candidate)&&prefs.getBytes("choices",&candidate,sizeof(candidate))==sizeof(candidate);prefs.end();
  // The display field was reserved in the previous on-disk format.
  if(found)candidate.upgrade();
  if(found&&candidate.valid())config=candidate;
  return !found||candidate.valid();
}
bool saveChoices(){
  if(!config.valid())return false;
  Preferences prefs;if(!prefs.begin("tab5-home",false))return false;
  bool ok=prefs.putBytes("choices",&config,sizeof(config))==sizeof(config);prefs.end();return ok;
}
bool reset(){
  endScan();used=0;++catalogueRevision;loadChoices();
  if(!apps)apps=static_cast<App*>(heap_caps_malloc(sizeof(App)*MaxApps,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  return apps!=nullptr;
}
unsigned count(){return used;}
uint32_t revision(){return catalogueRevision;}
const App* at(unsigned index){return index<used?apps+index:nullptr;}
bool addBuiltin(const char* id,const char* title,uint8_t action,uint8_t symbol){
  if(!apps||!id||strlen(id)>=sizeof(App::id))return false;
  App app;snprintf(app.id,sizeof(app.id),"%s",id);snprintf(app.title,sizeof(app.title),"%s",title);
  app.action=action;app.symbol=symbol;app.key=keyFor(id);return add(app);
}
bool beginScan(bool mounted){
  endScan();
  if(workerRunning.load(std::memory_order_acquire)){
    queuedScan=true;queuedSd=mounted;scanActive=true;return true;
  }
  entries=rootIndex=staged=0;truncated=failed=false;
  if(!apps){failed=true;return false;}
  if(!staging)staging=static_cast<App*>(heap_caps_malloc(sizeof(App)*MaxApps,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!staging){failed=true;return false;}
  for(unsigned i=0;i<used;++i)if(apps[i].kind==Kind::Builtin)staging[staged++]=apps[i];
  scanActive=true;scanSd=mounted;
  if(!mounted&&!Tab5UsbStorage::mounted()){publishScan(1);return false;}
  cancelScan.store(false,std::memory_order_release);workerResult.store(0,std::memory_order_relaxed);
  workerMode=true;workerRunning.store(true,std::memory_order_release);
  // Optional work must not trigger the recovering allocator's UI wait/purge.
  auto admission=Tab5MemoryManager.reserve(Tab5Resources::Owner::Tools,Tab5Resources::TaskBudget(ScanStack));
  if(admission&&tab5CreateAdmittedTask(admission,scanWorker,"tab5_catalogue",ScanStack,nullptr,1,nullptr,0)==pdPASS)return true;
  workerRunning.store(false,std::memory_order_release);workerMode=false;
  Tab5DebugSerial.printf("[APPS] Catalogue worker unavailable; incremental scan\n");
  // Preserve discovery when a transient task allocation cannot be admitted.
  if(startScanIo())return true;
  closeScanIo();scanActive=false;staged=0;return false;
}
void endScan(){
  queuedScan=false;scanActive=false;cancelScan.store(true,std::memory_order_release);
  if(workerRunning.load(std::memory_order_acquire))return;
  if(!workerMode)closeScanIo();
  workerMode=false;staged=0;
}
int scanStep(){
  if(queuedScan){
    if(workerRunning.load(std::memory_order_acquire))return 0;
    const bool mounted=queuedSd;queuedScan=false;
    return beginScan(mounted)?0:terminalResult();
  }
  if(workerMode){
    if(workerRunning.load(std::memory_order_acquire))return 0;
    return publishScan(workerResult.load(std::memory_order_relaxed));
  }
  if(!scanActive)return terminalResult();
  const int result=stepScanIo();
  if(result==0)return 0;
  closeScanIo();return publishScan(result);
}
static int stepScanIo(){
  if(!phase)return failed?-1:truncated?2:1;
  if(staged>=MaxApps||entries>=384){truncated=true;return terminalResult();}
  if(phase==1){
    Tab5PythonServices::AppInfo info;int result=Tab5PythonServices::nextApp(info);
    if(result==0)return 0;
    if(result==1){App app;snprintf(app.id,sizeof(app.id),"%s",info.path);snprintf(app.title,sizeof(app.title),"%s",info.title);
      app.key=keyFor(app.id);app.kind=Kind::Script;
      using I=Tab5PythonServices::AppIcon;
      switch(info.icon){case I::Workshop:app.symbol=2;break;case I::Libraries:app.symbol=8;break;case I::Map:app.symbol=4;break;case I::Audio:app.symbol=3;break;case I::Theme:app.symbol=7;break;case I::Storage:app.symbol=9;break;default:app.symbol=6;}
      // User-facing app labels describe the task, not its implementation.
      if((!strcmp(info.path,"/MicroPython/libraries.py")||!strcmp(info.path,"/MicroPython/bibliotheques.py")))snprintf(app.title,sizeof(app.title),"Modules et projets");
      else if((!strcmp(info.path,"/MicroPython/example.py")||!strcmp(info.path,"/MicroPython/exemple.py")))snprintf(app.title,sizeof(app.title),"Exemple");
      else if(!strcmp(info.path,"/MicroPython/interface.py"))snprintf(app.title,sizeof(app.title),"Interface exemple");
      add(app,true);return 0;}
    if(result<0)failed=true;if(result==3)truncated=true;phase=2;return 0;
  }
  if(!directory){
    if(rootIndex>=sizeof(roots)/sizeof(roots[0]))return terminalResult();
    const bool usb=rootIndex>=3;
    if(usb?!Tab5UsbStorage::mounted():!scanSd){++rootIndex;return 0;}
    errno=0;directory=tab5FS.open(roots[rootIndex],FILE_READ);
    if(!directory||!directory.isDirectory()){
      if(errno&&errno!=ENOENT)failed=true;
      directory.close();++rootIndex;
    }
    return 0;
  }
  errno=0;File entry=directory.openNextFile();
  if(!entry){if(errno&&errno!=ENOENT)failed=true;directory.close();++rootIndex;return 0;}
  ++entries;char leaf[272];snprintf(leaf,sizeof(leaf),"%s",entry.name());
  if(const char* slash=strrchr(leaf,'/'))memmove(leaf,slash+1,strlen(slash+1)+1);
  if(leaf[0]=='.')return 0;
  App app;int n=snprintf(app.id,sizeof(app.id),"%s/%s",roots[rootIndex],leaf);if(n<0||n>=int(sizeof(app.id)))return 0;
  const bool folder=entry.isDirectory();entry.close();
  if(folder){
    const char* filenames[]={"main.py","main.app.elf","main.elf"};bool found=false;
    for(unsigned i=leaf[0]=='_'?1:0;i<3;++i){char path[272];if(snprintf(path,sizeof(path),"%s/%s",app.id,filenames[i])>=int(sizeof(path)))continue;
      if(regular(path)){snprintf(app.id,sizeof(app.id),"%s",path);app.kind=i?Kind::Executable:Kind::Script;found=true;break;}}
    if(!found)return 0;
  }else{
    const size_t len=strlen(leaf);
    if(len>4&&!strcasecmp(leaf+len-4,".elf"))app.kind=Kind::Executable;
    // Top-level Python modules use the application manifest, not raw discovery.
    else if(rootIndex%3!=2&&leaf[0]!='_'&&len>3&&!strcasecmp(leaf+len-3,".py"))app.kind=Kind::Script;
    else return 0;
  }
  titleFor(app,leaf);app.key=keyFor(app.id);app.symbol=6;add(app,true);return 0;
}
bool pinned(unsigned index){
  if(index>=used)return false;if(!config.personal)return true;
  int pos=config.find(apps[index].key);return pos<0?true:bool(config.apps[pos].pinned);
}
uint8_t icon(unsigned index){if(index>=used)return 0;int pos=config.find(apps[index].key);return config.personal&&pos>=0?config.apps[pos].icon:0;}
bool seedCurrentOrder(){
  unsigned indices[MaxApps];const unsigned n=ordered(indices,false);
  for(unsigned i=0;i<n;++i){Choice* choice=config.get(apps[indices[i]].key,true);if(!choice)return false;choice->order=i;}
  return true;
}
bool pin(unsigned index,bool value){
  if(index>=used||!seedCurrentOrder())return false;
  Choice* c=config.get(apps[index].key,true);if(!c)return false;c->pinned=value;config.personal=1;return true;
}
bool setIcon(unsigned index,uint8_t symbol){
  if(index>=used||symbol>=SymbolCount||!seedCurrentOrder())return false;
  Choice* c=config.get(apps[index].key,true);if(!c)return false;c->icon=symbol;config.personal=1;return true;
}
unsigned ordered(unsigned* indices,bool onlyPinned){
  if(!indices)return 0;unsigned total=0;
  for(unsigned i=0;i<used;++i)if(!onlyPinned||pinned(i))indices[total++]=i;
  for(unsigned i=1;i<total;++i){unsigned value=indices[i],j=i;
    while(j&&before(value,indices[j-1])){indices[j]=indices[j-1];--j;}indices[j]=value;}
  return total;
}
bool move(unsigned index,int direction){
  if(index>=used||!pinned(index)||!direction)return false;
  unsigned order[MaxApps];unsigned n=ordered(order,true),position=0;
  while(position<n&&order[position]!=index)++position;
  if(position>=n)return false;
  // Moving an entry never crosses its native / ELF / Python section.
  int other=int(position)+(direction<0?-1:1);
  while(other>=0&&other<int(n)&&apps[order[other]].kind!=apps[index].kind)other+=direction<0?-1:1;
  if(other<0||other>=int(n))return false;
  if(!seedCurrentOrder())return false;
  config.personal=1;
  unsigned swap=order[other];order[other]=order[position];order[position]=swap;
  for(unsigned i=0;i<n;++i)config.get(apps[order[i]].key,true)->order=i;
  // Keep hidden applications after the pinned list, with unique stable ranks.
  unsigned next=n;for(unsigned i=0;i<config.count;++i)if(!config.apps[i].pinned)config.apps[i].order=next++;
  return true;
}
bool attachIcon(lv_obj_t* container,unsigned index){
  if(!container||index>=used||icon(index)||iconBytes>=MaxIconBytes)return false;
  Tab5StorageUse use(true);if(!use)return false;
  const App& app=apps[index];char candidate[288],base[272];snprintf(base,sizeof(base),"%s",app.id);
  char* leaf=strrchr(base,'/');leaf=leaf?leaf+1:base;
  size_t leafLength=strlen(leaf);
  if(leafLength>8&&!strcasecmp(leaf+leafLength-8,".app.elf"))leaf[leafLength-8]=0;
  else if(char* extension=strrchr(leaf,'.'))*extension=0;
  OwnedIcon* owned=nullptr;
  if(app.kind!=Kind::Builtin){
    if(snprintf(candidate,sizeof(candidate),"%s.ico",base)<int(sizeof(candidate)))owned=readIcon(candidate);
    if(!owned){char folder[272];snprintf(folder,sizeof(folder),"%s",app.id);char* slash=strrchr(folder,'/');if(slash)*slash=0;
      if(strcmp(folder,"/MicroPython")&&strcmp(folder,"/Applications")&&strcmp(folder,"/Apps")){
        snprintf(candidate,sizeof(candidate),"%s/icon.ico",folder);owned=readIcon(candidate);}}
  }
  if(!owned){
    char central[272];snprintf(central,sizeof(central),"%s",leaf);
    if(app.kind!=Kind::Builtin&&!strcmp(leaf,"main")){
      char folder[272];snprintf(folder,sizeof(folder),"%s",app.id);char* slash=strrchr(folder,'/');if(slash)*slash=0;
      const char* name=strrchr(folder,'/');if(name&&name[1])snprintf(central,sizeof(central),"%s",name+1);
    }
    snprintf(candidate,sizeof(candidate),"%s/Icons/%s.ico",Tab5UsbStorage::path(app.id)?"/USB":"",central);owned=readIcon(candidate);
  }
  if(!owned)return false;
  lv_obj_clean(container);auto* image=lv_image_create(container);lv_image_set_src(image,&owned->image);
  if(owned->image.header.w>48||owned->image.header.h>48){unsigned dimension=owned->image.header.w>owned->image.header.h?owned->image.header.w:owned->image.header.h;lv_image_set_scale(image,48*256/dimension);}
  lv_obj_center(image);lv_obj_add_event_cb(image,freeIcon,LV_EVENT_DELETE,owned);return true;
}
}
