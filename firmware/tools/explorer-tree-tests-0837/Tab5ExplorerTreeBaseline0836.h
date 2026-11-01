#pragma once
#include "Tab5VolumeFS.h"
#include "Tab5HiddenFiles.h"
#include "Tab5UsbStorage.h"
#include "Tab5DirectoryModel.h"
#include <new>
#include <string>

// Lazy Explorer tree. Only expanded directories own metadata; all persistent
// metadata lives in PSRAM. No UI, selection, preview or navigation state lives
// here. A chevron operation therefore cannot change either pane's directory.
class Tab5ExplorerTree {
 public:
  static constexpr size_t PathCapacity=260;
  struct Row {
    std::string path,name;
    bool directory=false,hidden=false;
    uint16_t depth=0;
    uint64_t bytes=0;
    int64_t modified=0;
  };
  Tab5ExplorerTree()=default;
  ~Tab5ExplorerTree(){clear();}
  Tab5ExplorerTree(const Tab5ExplorerTree&)=delete;
  Tab5ExplorerTree& operator=(const Tab5ExplorerTree&)=delete;
  bool begin(const String& path){
    clear();failed_=false;showHidden_=Tab5Files::showHidden;order_=Tab5FileSort::selected;
    usbMounted_=Tab5UsbStorage::mounted();
    if(!validPath(path.c_str())||!(root_=create(path.c_str(),nullptr,0))){failed_=true;return false;}
    dirty_=true;return start(root_);
  }
  // Explicit refresh is the only operation besides collapse/root change that
  // discards cached metadata. Expansion paths survive while still present.
  bool refresh(){
    if(!root_)return false;
    directory_.close();scanning_=nullptr;failed_=false;
    showHidden_=Tab5Files::showHidden;order_=Tab5FileSort::selected;usbMounted_=Tab5UsbStorage::mounted();
    for(Branch* b=root_;b;b=next(b)){b->model.clear();b->visible=0;b->state=State::Queued;}
    dirty_=true;return start(root_);
  }
  void clear(){directory_.close();scanning_=nullptr;destroy(root_);root_=nullptr;failed_=false;dirty_=true;}
  void cancel(){
    directory_.close();
    if(scanning_){scanning_->model.clear();scanning_->state=State::Queued;recount(scanning_);scanning_=nullptr;}
    dirty_=true;
  }
  const char* rootPath() const{return root_?root_->path:"";}
  size_t rowCount() const{return root_?root_->visible:0;}
  bool busy() const{return scanning_||pending(root_);}
  bool failed() const{return failed_;}
  bool dirty() const{return dirty_;}
  void clearDirty(){dirty_=false;}
  bool expanded(const char* path)const{return find(path)!=nullptr;}
  bool expanded(const String& path)const{return expanded(path.c_str());}
  bool expand(const String& path){return expand(path.c_str());}
  bool expand(const char* path){
    if(!root_||!validPath(path))return false;
    if(Branch* b=find(path))return b->state==State::Failed?retry(b):true;
    char parent[PathCapacity];const char* name=nullptr;if(!split(path,parent,name))return false;
    Branch* p=find(parent);if(!p||!ready(p))return false;
    const size_t at=indexOf(p,name);if(at==p->model.size()||!p->model[at].directory)return false;
    Branch* b=create(path,p,at);if(!b){failed_=true;dirty_=true;return false;}
    Branch** link=&p->child;while(*link&&(*link)->index<at)link=&(*link)->sibling;
    b->sibling=*link;*link=b;dirty_=true;return true;
  }
  bool collapse(const String& path){return collapse(path.c_str());}
  bool collapse(const char* path){
    Branch* b=find(path);if(!b||b==root_)return false;
    Branch* p=b->parent;Branch** link=&p->child;while(*link!=b)link=&(*link)->sibling;
    *link=b->sibling;b->sibling=nullptr;
    if(scanning_&&within(scanning_->path,b->path)){directory_.close();scanning_=nullptr;}
    destroy(b);recount(p);dirty_=true;return true;
  }
  bool toggle(const String& path){return toggle(path.c_str());}
  bool toggle(const char* path){return expanded(path)?collapse(path):expand(path);}
  // A removed volume must not leave a live File handle or stale descendants.
  // The root listing is refreshed only when it actually contains that volume.
  void invalidate(const char* path){
    if(!root_||!path)return;
    if(within(root_->path,path)){directory_.close();scanning_=nullptr;destroyChildren(root_);root_->model.clear();root_->visible=0;root_->state=State::Failed;failed_=true;dirty_=true;return;}
    collapse(path);
    if(!std::strcmp(root_->path,"/")&&!std::strcmp(path,"/USB"))refresh();
  }
  void invalidate(const String& path){invalidate(path.c_str());}
  Row row(size_t at)const{
    Row result;const Branch* b=root_;if(!b||at>=b->visible)return result;
    uint16_t depth=0;
    // Expanded children are sorted by their parent's entry index. Skip a whole
    // subtree or a run of siblings at once: large collapsed folders cost O(1).
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
      const auto& item=b->model[first];result.path=b->path;
      if(result.path!="/")result.path+='/';result.path+=item.name;
      result.name=item.name;result.directory=item.directory;result.depth=depth;
      result.bytes=item.bytes;result.modified=item.modified;
      result.hidden=Tab5HiddenFiles::hidden(result.path.c_str());return result;
    }
    return result;
  }
  void service(){
    if(!root_)return;
    const bool mounted=Tab5UsbStorage::mounted();
    if(usbMounted_&&!mounted){usbMounted_=false;invalidate("/USB");}
    if(order_!=Tab5FileSort::selected){order_=Tab5FileSort::selected;sort();}
    if(!scanning_){Branch* b=pending(root_);if(!b)return;if(!start(b))return;}
    const uint32_t started=millis();
    for(unsigned i=0;i<8&&millis()-started<6;++i){
      File item=directory_.openNextFile();
      if(!item){finish(false);return;}
      String name=item.name();const int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);
      const bool folder=item.isDirectory();const uint64_t bytes=item.size();const int64_t modified=item.getLastWrite();item.close();
      if(!name.length()||name=="."||name=="..")continue;
      char full[PathCapacity];if(!join(scanning_->path,name.c_str(),full))continue;
      if(!std::strcmp(scanning_->path,"/")&&!std::strcmp(name.c_str(),"USB")&&usbMounted_)continue;
      if(!showHidden_&&Tab5HiddenFiles::hidden(full))continue;
      if(!scanning_->model.add(name.c_str(),folder,bytes,modified)){finish(true);return;}
    }
  }
 private:
  enum class State:uint8_t{Queued,Scanning,Ready,Failed};
  struct Branch {
    char* path=nullptr;
    Branch *parent=nullptr,*child=nullptr,*sibling=nullptr;
    size_t index=0,visible=0;
    State state=State::Queued;
    Tab5DirectoryModel model;
  };
  Branch *root_=nullptr,*scanning_=nullptr;
  File directory_;
  bool dirty_=false,failed_=false,showHidden_=false,usbMounted_=false;
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
  // Destruction uses the tree links rather than recursive C++ stack frames.
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
      const char* name=std::strrchr(child->path,'/')+1;const size_t at=indexOf(b,name);
      if(at==b->model.size()||!b->model[at].directory){destroy(child);continue;}
      child->index=at;Branch** link=&b->child;while(*link&&(*link)->index<at)link=&(*link)->sibling;child->sibling=*link;*link=child;
    }
  }
  bool retry(Branch* b){b->model.clear();b->state=State::Queued;destroyChildren(b);recount(b);dirty_=true;return true;}
  bool start(Branch* b){
    directory_=tab5FS.open(b->path,FILE_READ);scanning_=b;b->state=State::Scanning;
    if(!directory_||!directory_.isDirectory()){finish(true);return false;}
    if(b==root_&&!std::strcmp(b->path,"/")&&usbMounted_&&!b->model.add("USB",true)){finish(true);return false;}
    return true;
  }
  void finish(bool failure){
    directory_.close();Branch* b=scanning_;scanning_=nullptr;if(!b)return;
    b->model.sort(order_);b->state=failure?State::Failed:State::Ready;failed_|=failure;
    relink(b);recount(b);dirty_=true;
  }
  void sort(){
    for(Branch* b=root_;b;b=next(b))if(ready(b)){b->model.sort(order_);relink(b);}
    dirty_=true;
  }
};
