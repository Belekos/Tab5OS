#include "Tab5FolderPreview.h"
#include <cassert>
#include <cstdio>
#include <set>

static std::shared_ptr<Node> directory(const char* path,size_t count){
  auto root=std::make_shared<Node>();root->name=path;root->directory=true;
  for(size_t i=0;i<count;++i){auto n=std::make_shared<Node>();n->name="file-"+std::to_string(i);n->directory=i%10==0;root->children.push_back(n);}
  tab5FS.nodes[path]=root;return root;
}
static unsigned finish(Tab5FolderPreview& preview){
  unsigned calls=0;while(!preview.done){preview.service();assert(++calls<10000);assert(preview.entries.size()<=Tab5FolderPreview::PageSize);}
  assert(activeHandles==0);return calls;
}
int main(){
  Tab5FolderPreview p;
  directory("/eight",8);assert(p.begin("/eight"));finish(p);
  assert(p.count==8&&p.entries.size()==8&&!p.failed);
  assert(p.entries[7].name=="file-7");
  directory("/fortynine",49);assert(p.begin("/fortynine"));finish(p);
  assert(p.count==49&&p.entries.size()==48);
  assert(p.begin("/fortynine",48));finish(p);
  assert(p.count==49&&p.entries.size()==1&&p.entries[0].name=="file-9");
  auto big=directory("/large",1007);
  assert(p.begin("/large"));const unsigned calls=finish(p);
  assert(calls>100&&p.count==1007&&p.modified==1234);
  size_t visited=0;std::set<std::string> paths;bool sawFile=false;
  for(size_t offset=0;offset<1007;offset+=48){
    assert(p.begin("/large",offset));finish(p);assert(p.count==1007);
    for(const auto& e:p.entries){assert(paths.insert(e.path.value).second);assert(e.path.value.find("/large/file-")==0);if(!e.directory)sawFile=true;else assert(!sawFile);++visited;}
  }
  assert(visited==1007);
  const int opens=filesystemOpens;assert(p.begin("/large",1007));finish(p);assert(p.count==1007&&p.entries.size()==47&&p.offset==960);assert(filesystemOpens==opens);
  auto dot=std::make_shared<Node>();dot->name=".secret";big->children.push_back(dot);
  auto self=std::make_shared<Node>();self->name=".";big->children.push_back(self);
  auto parent=std::make_shared<Node>();parent->name="..";big->children.push_back(parent);
  Tab5HiddenFiles::hiddenPaths.insert("/large/file-10");
  Tab5Files::showHidden=false;assert(p.begin("/large"));finish(p);
  assert(p.count==1006);for(const auto&e:p.entries)assert(!e.hidden&&e.name!="file-10");
  Tab5Files::showHidden=true;assert(p.begin("/large",960));finish(p);
  assert(p.count==1008&&p.entries.size()==48);
  assert(p.begin("/large",96));finish(p);bool foundHidden=false;for(const auto&e:p.entries)if(e.name==".secret")foundHidden=e.hidden;assert(foundHidden);
  assert(p.begin("/large"));p.service();assert(!p.done&&activeHandles==1);
  p.cancel();assert(p.done&&p.entries.empty()&&p.path.length()==0&&activeHandles==0);
  assert(p.begin("/large"));p.service();assert(p.begin("/eight"));finish(p);assert(p.count==8&&activeHandles==0);
  assert(!p.begin("/missing")&&p.done&&p.failed&&activeHandles==0);
  auto file=std::make_shared<Node>();file->name="plain";tab5FS.nodes["/plain"]=file;
  assert(!p.begin("/plain")&&p.done&&p.failed&&activeHandles==0);
  auto root=directory("/",1);root->children[0]->name="/file-0";
  assert(p.begin("/"));finish(p);assert(p.entries[0].path=="/file-0");
  assert(peakHandles<=2);
  std::puts("PASS: production Tab5FolderPreview.h, 8/49/1007 entries, every page, hidden filtering, empty tail, cancellation, restart and handle release.");
}
