#pragma once
#include "Tab5VolumeFS.h"
#include "Tab5HiddenFiles.h"
#include "Tab5UsbStorage.h"
#include "Tab5DirectoryModel.h"
#include <new>
#include <string>

// Only folders belong in the navigation tree. Expanded branches and the small
// cache of requested child-presence hints live in PSRAM. No recursive scans or
// synchronous probes run while a row is being created.
class Tab5ExplorerTree {
 public:
  static constexpr size_t PathCapacity=260;
  struct Row {
    std::string path,name;
    bool directory=false,hidden=false,volume=false,expandable=false;
    uint16_t depth=0;
    uint64_t bytes=0;
    int64_t modified=0;
  };
  Tab5ExplorerTree()=default;
  ~Tab5ExplorerTree(){clear();}
  Tab5ExplorerTree(const Tab5ExplorerTree&)=delete;
  Tab5ExplorerTree& operator=(const Tab5ExplorerTree&)=delete;
  bool begin(const String& path){
    clear();showHidden_=Tab5Files::showHidden;order_=Tab5FileSort::selected;usbMounted_=Tab5UsbStorage::mounted();
    if(!validPath(path.c_str())||!(root_=create(path.c_str(),nullptr,0))){failed_=true;return false;}
    settled_=false;dirty_=true;return start(root_);
  }
  // A synthetic invisible parent owns the visible SD and USB roots. A support
  // is a normal focusable tree row; USB is never nested inside the SD listing.
  bool beginVolumes(bool sdReady){
    clear();volumes_=true;showHidden_=Tab5Files::showHidden;order_=Tab5FileSort::selected;
    root_=create("",nullptr,0);if(!root_){failed_=true;return false;}
    root_->state=State::Ready;
    sdAvailable_=sdReady;usbMounted_=Tab5UsbStorage::mounted();
    if(!buildVolumes())return false;
    const char* first=sdReady?"/":usbMounted_?"/USB":nullptr;
    if(first&&!expand(first))return false;
    Branch* pendingBranch=pending(root_);return !pendingBranch||start(pendingBranch);
  }
  bool syncVolumes(bool sdReady){
    if(!volumes_||!root_)return beginVolumes(sdReady);
    const bool usbReady=Tab5UsbStorage::mounted();
    if(sdReady==sdAvailable_&&usbReady==usbMounted_)return true;
    sdAvailable_=sdReady;usbMounted_=usbReady;
    if(scanning_&&!supportAvailable(scanning_->path)){directory_.close();scanning_=nullptr;}
    clearProbes();return buildVolumes();
  }
  bool volumes()const{return volumes_;}
  // Explicit refresh rescans expanded branches only; their paths and the
  // collapsed state of each support survive. Known hints are invalidated.
  bool refresh(){
    if(!root_)return false;
    directory_.close();scanning_=nullptr;clearProbes();failed_=false;
    showHidden_=Tab5Files::showHidden;order_=Tab5FileSort::selected;
    if(volumes_){if(!syncVolumes(sdAvailable_))return false;}
    else usbMounted_=Tab5UsbStorage::mounted();
    for(Branch* b=root_;b;b=next(b)){
      if(volumes_&&b==root_)continue;
      b->model.clear();b->visible=0;b->state=State::Queued;
    }
    recount(root_);settled_=pending(root_)==nullptr;dirty_=true;
    Branch* b=pending(root_);return !b||start(b);
  }
  // Resource pressure may discard directory metadata, never session expansion
  // paths. The next visible service pass rebuilds the same expanded branches.
  void releaseMetadata(){
    directory_.close();scanning_=nullptr;clearProbes();
    for(Branch* b=root_;b;b=next(b)){
      if(volumes_&&b==root_)continue;
      b->model.clear();b->visible=0;b->state=State::Queued;
    }
    recount(root_);settled_=pending(root_)==nullptr;dirty_=true;
  }
  void clear(){
    directory_.close();scanning_=nullptr;clearProbes();destroy(root_);root_=nullptr;
    failed_=false;settled_=true;dirty_=true;volumes_=false;sdAvailable_=false;usbMounted_=false;
  }
  void cancel(){
    directory_.close();
    if(scanning_){
      scanning_->model.clear();scanning_->state=State::Queued;recount(scanning_);scanning_=nullptr;
      settled_=false;dirty_=true;
    }
    if(probing_){probeDirectory_.close();probing_->state=Hint::Queued;queue(probing_);probing_=nullptr;}
    // A settled tree has nothing to cancel. Keep its rows, scroll and cached
    // hints intact when leaving an application and returning during a session.
  }
  const char* rootPath()const{return root_?(volumes_?"/":root_->path):"";}
  size_t rowCount()const{return root_?root_->visible:0;}
  bool busy()const{return scanning_||!settled_;}
  bool failed()const{return failed_;}
  bool dirty()const{return dirty_;}
  void clearDirty(){dirty_=false;}
  bool hintsDirty()const{return hintsDirty_;}
  void clearHintsDirty(){hintsDirty_=false;}
  bool probing()const{return probing_||probeFirst_;}
  bool expanded(const char* path)const{return path&&path[0]&&find(path)!=nullptr;}
  bool expanded(const String& path)const{return expanded(path.c_str());}
  bool hasSubdirectories(const String& path)const{return hasSubdirectories(path.c_str());}
  bool hasSubdirectories(const char* path)const{
    if(!path)return false;
    if(const Probe* p=findProbe(path))return p->state==Hint::Yes;
    const Branch* b=find(path);return b&&ready(b)&&b->model.size()>0;
  }
  void requestChildProbe(const String& path){requestChildProbe(path.c_str());}
  void requestChildProbe(const char* path){
    if(!root_||!validPath(path))return;
    if(Probe* existing=findProbe(path)){
      if(existing->state==Hint::Unrequested){existing->state=Hint::Queued;queue(existing);}
      return;
    }
    // The caller requests only visible rows; never discover descendants here.
    size_t visibleIndex=0;if(!rowIndex(path,visibleIndex)&&std::strcmp(rootPath(),path))return;
    Probe* p=createProbe(path);if(!p)return;
    const Branch* b=find(path);
    if(b&&ready(b)){p->state=b->model.size()?Hint::Yes:Hint::No;hintsDirty_=true;return;}
    // A pending normal scan will answer this without opening another handle.
    p->state=Hint::Queued;queue(p);
  }
  bool expand(const String& path){return expand(path.c_str());}
  bool expand(const char* path){
    if(!root_||!validPath(path))return false;
    if(Branch* b=find(path))return b->state==State::Failed?retry(b):true;
    Branch* parent=nullptr;size_t at=0;
    if(volumes_&&isVolume(path)){parent=root_;at=indexOf(root_,path);}
    else {
      char parentPath[PathCapacity];const char* name=nullptr;if(!split(path,parentPath,name))return false;
      parent=find(parentPath);if(!parent||!ready(parent))return false;at=indexOf(parent,name);
    }
    if(at==parent->model.size())return false;
    Branch* b=create(path,parent,at);if(!b){failed_=true;dirty_=true;return false;}
    Branch** link=&parent->child;while(*link&&(*link)->index<at)link=&(*link)->sibling;
    b->sibling=*link;*link=b;settled_=false;dirty_=true;return true;
  }
  bool collapse(const String& path){return collapse(path.c_str());}
  bool collapse(const char* path){
    Branch* b=find(path);if(!b||b==root_)return false;
    Branch* p=b->parent;Branch** link=&p->child;while(*link!=b)link=&(*link)->sibling;
    *link=b->sibling;b->sibling=nullptr;
    if(scanning_&&within(scanning_->path,b->path)&&(!volumes_||sameSupport(scanning_->path,b->path))){directory_.close();scanning_=nullptr;}
    // Descendants are no longer visible; do not keep probing their files.
    for(Probe* hint=probes_;hint;hint=hint->allNext)if(std::strcmp(hint->path,path)&&within(hint->path,path)&&(!volumes_||sameSupport(hint->path,path))){
      if(hint==probing_){probeDirectory_.close();probing_=nullptr;}
      if(hint->state==Hint::Queued||hint->state==Hint::Active)hint->state=Hint::Unrequested;
    }
    destroy(b);recount(p);settled_=scanning_==nullptr&&pending(root_)==nullptr;dirty_=true;return true;
  }
  bool toggle(const String& path){return toggle(path.c_str());}
  bool toggle(const char* path){return expanded(path)?collapse(path):expand(path);}
  void invalidate(const char* path){
    if(!root_||!path)return;
    clearProbes();
    if(volumes_){
      if(isVolume(path)){
        if(scanning_&&sameSupport(scanning_->path,path)){directory_.close();scanning_=nullptr;}
        if(!std::strcmp(path,"/USB"))usbMounted_=false;else sdAvailable_=false;
        buildVolumes();return;
      }
      collapse(path);return;
    }
    if(within(root_->path,path)){directory_.close();scanning_=nullptr;destroyChildren(root_);root_->model.clear();root_->visible=0;root_->state=State::Failed;failed_=true;settled_=true;dirty_=true;return;}
    collapse(path);
  }
  void invalidate(const String& path){invalidate(path.c_str());}
  Row row(size_t at)const{
    Row result;const Branch* b=root_;if(!b||at>=b->visible)return result;
    uint16_t depth=0;
    while(b){
      size_t first=0;bool descend=false;
      for(const Branch* child=b->child;child;child=child->sibling){
        const size_t run=child->index-first+1;
        if(at<run){first+=at;goto found;}
        at-=run;
        if(at<child->visible){b=child;++depth;descend=true;break;}
        at-=child->visible;first=child->index+1;
      }
      if(descend)continue;
      first+=at;
      found:
      if(first>=b->model.size())return result;
      const auto& item=b->model[first];
      result.volume=volumes_&&b==root_;
      if(result.volume)result.path=item.name;
      else {result.path=b->path;if(result.path!="/")result.path+='/';result.path+=item.name;}
      result.name=item.name;result.directory=true;result.depth=depth;
      result.bytes=item.bytes;result.modified=item.modified;
      result.hidden=!result.volume&&Tab5HiddenFiles::hidden(result.path.c_str());
      result.expandable=hasSubdirectories(result.path.c_str());return result;
    }
    return result;
  }
  bool rowIndex(const String& path,size_t& at)const{return rowIndex(path.c_str(),at);}
  bool rowIndex(const char* path,size_t& at)const{
    if(!root_||!validPath(path))return false;
    const Branch* branch=nullptr;size_t item=0;
    if(volumes_&&isVolume(path)){branch=root_;item=indexOf(root_,path);}
    else {
      char parent[PathCapacity];const char* name=nullptr;if(!split(path,parent,name))return false;
      branch=find(parent);if(!branch||!ready(branch))return false;item=indexOf(branch,name);
    }
    if(item==branch->model.size())return false;
    size_t index=item;
    for(const Branch* child=branch->child;child&&child->index<item;child=child->sibling)index+=child->visible;
    while(branch->parent){
      const Branch* above=branch->parent;if(!ready(above))return false;
      index+=branch->index+1;
      for(const Branch* child=above->child;child&&child->index<branch->index;child=child->sibling)index+=child->visible;
      branch=above;
    }
    at=index;return true;
  }
  void service(){
    if(!root_)return;
    const bool mounted=Tab5UsbStorage::mounted();
    if(volumes_){if(usbMounted_!=mounted)syncVolumes(sdAvailable_);}
    else if(usbMounted_&&!mounted){usbMounted_=false;invalidate("/USB");}
    if(order_!=Tab5FileSort::selected){order_=Tab5FileSort::selected;sort();}
    if(!settled_){
      if(!scanning_){Branch* b=pending(root_);if(!b){settled_=true;return;}if(!start(b))return;}
      const uint32_t started=millis();
      for(unsigned i=0;i<8&&millis()-started<6;++i){
        File item=directory_.openNextFile();
        if(!item){finish(false);return;}
        // Files do not consume metadata, sort time, strings, or hidden lookups.
        if(!item.isDirectory()){item.close();continue;}
        String name=item.name();const int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);
        const uint64_t bytes=item.size();const int64_t modified=item.getLastWrite();item.close();
        if(!name.length()||name=="."||name=="..")continue;
        char full[PathCapacity];if(!join(scanning_->path,name.c_str(),full))continue;
        if(volumes_&&!std::strcmp(scanning_->path,"/")&&!std::strcmp(name.c_str(),"USB"))continue;
        if(!showHidden_&&Tab5HiddenFiles::hidden(full))continue;
        if(!scanning_->model.add(name.c_str(),true,bytes,modified)){finish(true);return;}
      }
      return;
    }
    serviceProbe();
  }
 private:
  enum class State:uint8_t{Queued,Scanning,Ready,Failed};
  enum class Hint:uint8_t{Queued,Active,No,Yes,Failed,Unrequested};
  struct Branch {
    char* path=nullptr;Branch *parent=nullptr,*child=nullptr,*sibling=nullptr;
    size_t index=0,visible=0;State state=State::Queued;Tab5DirectoryModel model;
  };
  struct Probe {
    char* path=nullptr;Probe *hashNext=nullptr,*allNext=nullptr,*queueNext=nullptr;
    Hint state=Hint::Queued;bool enqueued=false;
  };
  static constexpr unsigned ProbeBuckets=64;
  Branch *root_=nullptr,*scanning_=nullptr;
  Probe* probeBuckets_[ProbeBuckets]{};
  Probe *probes_=nullptr,*probeFirst_=nullptr,*probeLast_=nullptr,*probing_=nullptr;
  File directory_,probeDirectory_;
  bool dirty_=false,failed_=false,showHidden_=false,usbMounted_=false,settled_=true;
  bool volumes_=false,sdAvailable_=false,hintsDirty_=false;
  Tab5FileSort::Order order_=Tab5FileSort::Order::NameAscending;
  static void* allocate(size_t bytes){
#ifdef ARDUINO
    return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    return std::malloc(bytes);
#endif
  }
  static void release(void* pointer){
#ifdef ARDUINO
    heap_caps_free(pointer);
#else
    std::free(pointer);
#endif
  }
  static bool validPath(const char* path){return path&&path[0]=='/'&&std::strlen(path)<PathCapacity&&(!path[1]||path[std::strlen(path)-1]!='/');}
  static bool within(const char* path,const char* ancestor){const size_t n=std::strlen(ancestor);return !std::strncmp(path,ancestor,n)&&(n==1||!path[n]||path[n]=='/');}
  static bool isUsb(const char* path){return !std::strcmp(path,"/USB")||!std::strncmp(path,"/USB/",5);}
  static bool isVolume(const char* path){return !std::strcmp(path,"/")||!std::strcmp(path,"/USB");}
  static bool sameSupport(const char* path,const char* volume){return isUsb(path)==isUsb(volume);}
  bool supportAvailable(const char* path)const{return isUsb(path)?usbMounted_:sdAvailable_;}
  static bool split(const char* path,char* parent,const char*& name){
    const char* slash=std::strrchr(path,'/');if(!slash||!slash[1])return false;
    size_t length=size_t(slash-path);if(!length)length=1;std::memcpy(parent,path,length);parent[length]=0;name=slash+1;return true;
  }
  static bool join(const char* parent,const char* name,char* output){
    const size_t a=std::strlen(parent),b=std::strlen(name),slash=a==1?0:1;
    if(!b||a+slash+b>=PathCapacity)return false;
    std::memcpy(output,parent,a);if(slash)output[a]='/';std::memcpy(output+a+slash,name,b+1);return true;
  }
  static Branch* create(const char* path,Branch* parent,size_t index){
    void* storage=allocate(sizeof(Branch));if(!storage)return nullptr;
    Branch* b=new(storage) Branch;b->parent=parent;b->index=index;
    b->path=static_cast<char*>(allocate(std::strlen(path)+1));
    if(!b->path){b->~Branch();release(b);return nullptr;}
    std::strcpy(b->path,path);return b;
  }
  static void destroy(Branch* root){
    Branch* b=root;
    while(b){if(b->child){b=b->child;continue;}Branch* p=b==root?nullptr:b->parent;
      if(p)p->child=b->sibling;release(b->path);b->~Branch();release(b);b=p;}
  }
  static void destroyChildren(Branch* b){while(b->child){Branch* child=b->child;b->child=child->sibling;child->sibling=nullptr;destroy(child);}}
  static Branch* next(Branch* b){if(b->child)return b->child;while(b&&!b->sibling)b=b->parent;return b?b->sibling:nullptr;}
  Branch* find(const char* path)const{if(!path)return nullptr;for(Branch* b=root_;b;b=next(b))if(!std::strcmp(b->path,path))return b;return nullptr;}
  static bool ready(const Branch* b){return b->state==State::Ready||b->state==State::Failed;}
  static size_t indexOf(const Branch* b,const char* name){for(size_t i=0;i<b->model.size();++i)if(!std::strcmp(b->model[i].name,name))return i;return b->model.size();}
  static Branch* pending(Branch* root){
    Branch* b=root;
    while(b){if(b->state==State::Queued)return b;if(ready(b)&&b->child){b=b->child;continue;}
      while(b!=root&&!b->sibling)b=b->parent;if(b==root)return nullptr;b=b->sibling;}
    return nullptr;
  }
  static void recount(Branch* b){while(b){b->visible=ready(b)?b->model.size():0;if(ready(b))for(Branch* c=b->child;c;c=c->sibling)b->visible+=c->visible;b=b->parent;}}
  void relink(Branch* b){
    Branch* children=b->child;b->child=nullptr;
    while(children){Branch* child=children;children=children->sibling;child->sibling=nullptr;
      const char* name=volumes_&&b==root_?child->path:std::strrchr(child->path,'/')+1;const size_t at=indexOf(b,name);
      if(at==b->model.size()){destroy(child);continue;}
      child->index=at;Branch** link=&b->child;while(*link&&(*link)->index<at)link=&(*link)->sibling;child->sibling=*link;*link=child;
    }
  }
  bool buildVolumes(){
    root_->model.clear();bool ok=true;
    if(sdAvailable_)ok=root_->model.add("/",true)&&ok;
    if(usbMounted_)ok=root_->model.add("/USB",true)&&ok;
    root_->state=State::Ready;failed_|=!ok;
    if(scanning_){Branch* support=scanning_;while(support->parent&&support->parent!=root_)support=support->parent;
      if(indexOf(root_,support->path)==root_->model.size()){directory_.close();scanning_=nullptr;}}
    relink(root_);recount(root_);
    settled_=scanning_==nullptr&&pending(root_)==nullptr;dirty_=true;hintsDirty_=true;return ok;
  }
  bool retry(Branch* b){b->model.clear();b->state=State::Queued;destroyChildren(b);recount(b);settled_=false;dirty_=true;return true;}
  bool start(Branch* b){
    // An expansion scan also answers its child-presence hint. Do not hold a
    // second iterator on the same directory; retain the queued hint only if
    // this expansion is cancelled before it can answer.
    if(probing_&&!std::strcmp(probing_->path,b->path)){
      probeDirectory_.close();probing_->state=Hint::Queued;queue(probing_);probing_=nullptr;
    }
    directory_=tab5FS.open(b->path,FILE_READ);scanning_=b;b->state=State::Scanning;
    if(!directory_||!directory_.isDirectory()){finish(true);return false;}return true;
  }
  void finish(bool failure){
    directory_.close();Branch* b=scanning_;scanning_=nullptr;if(!b)return;
    b->model.sort(order_);b->state=failure?State::Failed:State::Ready;failed_|=failure;
    relink(b);recount(b);settled_=pending(root_)==nullptr;dirty_=true;
    if(Probe* p=findProbe(b->path)){p->state=b->model.size()?Hint::Yes:failure?Hint::Failed:Hint::No;hintsDirty_=true;}
  }
  void sort(){
    for(Branch* b=root_;b;b=next(b))if(ready(b)&&!(volumes_&&b==root_)){b->model.sort(order_);relink(b);}
    dirty_=true;
  }
  static unsigned hashPath(const char* path){uint32_t hash=2166136261u;while(*path){hash^=uint8_t(*path++);hash*=16777619u;}return hash&(ProbeBuckets-1);}
  Probe* findProbe(const char* path)const{
    for(Probe* p=probeBuckets_[hashPath(path)];p;p=p->hashNext)if(!std::strcmp(p->path,path))return p;
    return nullptr;
  }
  Probe* createProbe(const char* path){
    void* storage=allocate(sizeof(Probe));if(!storage)return nullptr;Probe* p=new(storage) Probe;
    p->path=static_cast<char*>(allocate(std::strlen(path)+1));if(!p->path){p->~Probe();release(p);return nullptr;}
    std::strcpy(p->path,path);const unsigned hash=hashPath(path);p->hashNext=probeBuckets_[hash];probeBuckets_[hash]=p;
    p->allNext=probes_;probes_=p;return p;
  }
  void queue(Probe* p){if(p->enqueued)return;p->enqueued=true;p->queueNext=nullptr;if(probeLast_)probeLast_->queueNext=p;else probeFirst_=p;probeLast_=p;}
  void clearProbes(){
    probeDirectory_.close();probing_=probeFirst_=probeLast_=nullptr;
    while(probes_){Probe* p=probes_;probes_=p->allNext;release(p->path);p->~Probe();release(p);}
    for(auto& p:probeBuckets_)p=nullptr;hintsDirty_=true;
  }
  void finishProbe(Hint result){probeDirectory_.close();probing_->state=result;probing_=nullptr;hintsDirty_=true;}
  void serviceProbe(){
    if(!probing_){
      while(probeFirst_&&probeFirst_->state!=Hint::Queued){Probe* skipped=probeFirst_;probeFirst_=skipped->queueNext;skipped->enqueued=false;skipped->queueNext=nullptr;if(!probeFirst_)probeLast_=nullptr;}
      if(!probeFirst_)return;
      Probe* p=probeFirst_;probeFirst_=p->queueNext;if(!probeFirst_)probeLast_=nullptr;p->queueNext=nullptr;p->enqueued=false;
      if(const Branch* b=find(p->path);b&&ready(b)){p->state=b->model.size()?Hint::Yes:b->state==State::Failed?Hint::Failed:Hint::No;hintsDirty_=true;return;}
      probing_=p;p->state=Hint::Active;probeDirectory_=tab5FS.open(p->path,FILE_READ);
      if(!probeDirectory_||!probeDirectory_.isDirectory()){finishProbe(Hint::Failed);return;}
    }
    const uint32_t began=millis();
    for(unsigned i=0;i<8&&millis()-began<2;++i){
      File item=probeDirectory_.openNextFile();if(!item){finishProbe(Hint::No);return;}
      if(!item.isDirectory()){item.close();continue;}
      String name=item.name();const int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);item.close();
      if(!name.length()||name=="."||name=="..")continue;
      if(volumes_&&!std::strcmp(probing_->path,"/")&&!std::strcmp(name.c_str(),"USB"))continue;
      char full[PathCapacity];if(!join(probing_->path,name.c_str(),full))continue;
      if(!showHidden_&&Tab5HiddenFiles::hidden(full))continue;
      finishProbe(Hint::Yes);return;
    }
  }
};
