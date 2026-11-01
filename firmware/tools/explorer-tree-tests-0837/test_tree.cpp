#include "Tab5ExplorerTree.h"
#define Tab5ExplorerTree Tab5ExplorerTreeBaseline0836
#include "Tab5ExplorerTreeBaseline0836.h"
#undef Tab5ExplorerTree
#include <chrono>
#include <cassert>
#include <cstdio>
#include <set>
#include <algorithm>

static std::shared_ptr<Node> dir(const std::string& path){auto n=std::make_shared<Node>();n->name=path;n->directory=true;tab5FS.nodes[path]=n;return n;}
static std::shared_ptr<Node> add(const std::string& parent,const std::string& name,bool folder=false,uint64_t bytes=0,int64_t modified=1234){
  auto n=std::make_shared<Node>();n->name=name;n->directory=folder;n->bytes=bytes;n->modified=modified;
  const auto path=parent+(parent=="/"?"":"/")+name;tab5FS.nodes[path]=n;tab5FS.nodes.at(parent)->children.push_back(n);return n;
}
template<class Tree> static void finish(Tree& tree){unsigned calls=0;while(tree.busy()){int before=reads;tree.service();assert(reads-before<=8);assert(++calls<100000);}assert(activeHandles==0);}
static size_t index(Tab5ExplorerTree& tree,const std::string& path){size_t at=tree.rowCount();if(tree.rowIndex(path.c_str(),at))assert(tree.row(at).path==path);return at;}
static void validateIndices(Tab5ExplorerTree& tree){
 for(size_t i=0;i<tree.rowCount();++i){auto path=tree.row(i).path;size_t actual=1234567;assert(tree.rowIndex(path.c_str(),actual));assert(actual==i);}
 size_t unchanged=42;assert(!tree.rowIndex("/missing/never",unchanged)&&unchanged==42);assert(!tree.rowIndex("relative",unchanged)&&unchanged==42);assert(!tree.rowIndex(tree.rootPath(),unchanged));
}
static void reset(){assert(activeHandles==0);assert(allocations.empty());tab5FS.nodes.clear();openedPaths.clear();Tab5Files::showHidden=false;Tab5HiddenFiles::hiddenPaths.clear();Tab5UsbStorage::isMounted=false;Tab5FileSort::selected=Tab5FileSort::Order::NameAscending;failAfter=-1;}
static void basic(){
  reset();dir("/");add("/","z.txt");add("/","B",true);add("/","A",true);add("/A","inside.txt");add("/A","nested",true);add("/A/nested","deep.txt");add("/B","other.txt");
  Tab5ExplorerTree tree;assert(tree.begin("/"));assert(tree.rowCount()==0);finish(tree);assert(tree.rowCount()==3&&tree.row(0).name=="A");
  assert(openedPaths==std::vector<std::string>{"/"});const std::string root=tree.rootPath();
  tree.clearDirty();assert(tree.expand("/A")&&tree.dirty());finish(tree);
  assert(tree.rowCount()==5&&tree.row(1).path=="/A/nested"&&tree.row(1).depth==1&&tree.row(2).path=="/A/inside.txt");
  assert(tree.expand("/A/nested"));assert(tree.expand("/B"));finish(tree);
  assert(tree.rowCount()==7&&tree.row(2).path=="/A/nested/deep.txt"&&tree.row(2).depth==2);
  const size_t expandedBytes=liveBytes;const int opens=filesystemOpens;assert(tree.collapse("/A"));assert(tree.rowCount()==4&&tree.expanded("/B")&&!tree.expanded("/A/nested"));assert(liveBytes<expandedBytes);assert(filesystemOpens==opens&&tree.rootPath()==root);
  assert(!tree.expand("/z.txt")&&!tree.expand("/A/nested")&&!tree.collapse("/"));
  assert(tree.expand("/A"));finish(tree);assert(!tree.expanded("/A/nested"));
  assert(tree.row(tree.rowCount()).path.empty());tree.clear();assert(allocations.empty());
}
static void sortingRefresh(){
  reset();dir("/");add("/","z",true);add("/","a",true);add("/","file",false,900,60);add("/a","x",false,100,2);add("/a","y",false,200,1);add("/z","zchild");
  Tab5ExplorerTree tree;assert(tree.begin("/"));finish(tree);assert(tree.expand("/a")&&tree.expand("/z"));finish(tree);
  for(unsigned order=0;order<6;++order){Tab5FileSort::selected=static_cast<Tab5FileSort::Order>(order);tree.service();assert(tree.rowCount()==6);assert(tree.row(index(tree,"/a/x")).depth==1);assert(tree.row(index(tree,"/z/zchild")).depth==1);assert(tree.expanded("/a")&&tree.expanded("/z"));validateIndices(tree);}
  Tab5FileSort::selected=Tab5FileSort::Order::NameAscending;assert(tree.refresh());finish(tree);assert(tree.expanded("/a")&&tree.expanded("/z")&&tree.rowCount()==6);
  tab5FS.nodes["/"]->children.erase(tab5FS.nodes["/"]->children.begin());assert(tree.refresh());finish(tree);assert(!tree.expanded("/z")&&tree.expanded("/a")&&tree.rowCount()==4);
  tree.clear();assert(allocations.empty());
}
static void large(){
  reset();dir("/large");for(unsigned i=0;i<8193;++i)add("/large","file-"+std::to_string(i),false,i,i);
  add("/large",".secret");add("/large",".");add("/large","..");Tab5HiddenFiles::hiddenPaths.insert("/large/file-100");
  Tab5ExplorerTree tree;assert(tree.begin("/large"));finish(tree);assert(tree.rowCount()==8192);
  std::set<std::string> seen;const int opens=filesystemOpens;for(size_t page=0;page<tree.rowCount();page+=128)for(size_t i=page;i<std::min(page+128,tree.rowCount());++i){auto row=tree.row(i);assert(row.depth==0&&!row.hidden);assert(seen.insert(row.path).second);}
  assert(filesystemOpens==opens);validateIndices(tree);Tab5Files::showHidden=true;assert(tree.refresh());finish(tree);assert(tree.rowCount()==8194);assert(tree.row(index(tree,"/large/.secret")).hidden);
  tree.clear();assert(allocations.empty());
}
static void cancellationAndRemoval(){
  reset();dir("/");add("/","A",true);add("/","B",true);add("/B","keep");for(unsigned i=0;i<60;++i)add("/A","f"+std::to_string(i));
  Tab5ExplorerTree tree;assert(tree.begin("/"));finish(tree);assert(tree.expand("/B"));finish(tree);assert(tree.expand("/A"));tree.service();assert(activeHandles==1);assert(tree.collapse("/A"));assert(activeHandles==0&&tree.expanded("/B")&&tree.rowCount()==3);
  assert(tree.expand("/A"));tree.service();tree.cancel();assert(activeHandles==0&&tree.expanded("/A")&&tree.expanded("/B")&&tree.rootPath()==std::string("/"));assert(tree.refresh());finish(tree);assert(tree.rowCount()==63);
  Tab5UsbStorage::isMounted=true;dir("/USB");add("/USB","folder",true);for(unsigned i=0;i<60;++i)add("/USB/folder","u"+std::to_string(i));assert(tree.refresh());finish(tree);
  assert(tree.expand("/USB"));finish(tree);assert(tree.expand("/USB/folder"));tree.service();assert(activeHandles==1);Tab5UsbStorage::isMounted=false;tree.service();finish(tree);
  assert(activeHandles==0&&!tree.expanded("/USB")&&index(tree,"/USB")==tree.rowCount()&&tree.expanded("/B"));
  Tab5UsbStorage::isMounted=true;assert(tree.begin("/USB"));finish(tree);Tab5UsbStorage::isMounted=false;tree.service();assert(tree.failed()&&!tree.busy()&&tree.rowCount()==0&&activeHandles==0);
  tree.clear();assert(allocations.empty());assert(!tree.begin("/missing")&&tree.failed()&&!tree.busy());tree.clear();
}
static void allocationFailures(){
  reset();dir("/");add("/","folder",true);for(unsigned i=0;i<150;++i)add("/folder","child-"+std::to_string(i));
  for(long failure=0;failure<15;++failure){Tab5ExplorerTree tree;failAfter=failure;tree.begin("/");finish(tree);if(!tree.failed()){tree.expand("/folder");finish(tree);}assert(tree.failed());tree.clear();assert(allocations.empty()&&activeHandles==0);failAfter=-1;}
  Tab5ExplorerTree tree;assert(tree.begin("/"));finish(tree);const auto bytes=liveBytes;failAfter=0;assert(!tree.expand("/folder")&&tree.rowCount()==1&&liveBytes==bytes);failAfter=-1;assert(tree.expand("/folder"));finish(tree);assert(tree.rowCount()==151);tree.clear();assert(allocations.empty());
}
static void depthAndPaths(){
  reset();dir("/");std::string path="/";for(unsigned depth=0;depth<129;++depth){add(path,"a",true);path+=(path=="/"?"":"/");path+="a";}
  add(path,"z"); // /a repeated 129 + /z is 260 bytes, outside the OS path capacity.
  Tab5ExplorerTree tree;assert(tree.begin("/"));finish(tree);path="/";
  for(unsigned depth=0;depth<129;++depth){path+=(path=="/"?"":"/");path+="a";assert(tree.expand(path.c_str()));finish(tree);assert(tree.row(depth).depth==depth);}
  validateIndices(tree);assert(tree.rowCount()==129&&path.size()==258);assert(tree.collapse("/a")&&tree.rowCount()==1);tree.clear();assert(allocations.empty());
  assert(!tree.begin(std::string(260,'/').c_str()));tree.clear();assert(allocations.empty());
}
static void performance(){
 reset();dir("/");for(unsigned i=0;i<512;++i){char name[40];snprintf(name,sizeof(name),"directory-%04u",i);add("/",name,true);}
 for(unsigned i=0;i<8193;++i){char name[40];snprintf(name,sizeof(name),"file-%05u.txt",i);add("/",name);}
 Tab5ExplorerTree current;Tab5ExplorerTreeBaseline0836 baseline;
 assert(current.begin("/"));finish(current);assert(baseline.begin("/"));finish(baseline);
 for(unsigned i=0;i<512;++i){char path[40];snprintf(path,sizeof(path),"/directory-%04u",i);assert(current.expand(path)&&baseline.expand(path));}
 finish(current);finish(baseline);assert(current.rowCount()==baseline.rowCount());validateIndices(current);
 const int oldReads=reads,oldOpens=filesystemOpens;const size_t oldBytes=liveBytes;
 auto now=[](){return std::chrono::steady_clock::now();};
 auto elapsed=[](auto start){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();};
 auto start=now();for(unsigned i=0;i<200000;++i){assert(!baseline.busy());baseline.service();}const auto baselineIdleNs=elapsed(start);
 start=now();for(unsigned i=0;i<200000;++i){assert(!current.busy());current.service();}const auto currentIdleNs=elapsed(start);
 assert(reads==oldReads&&filesystemOpens==oldOpens&&liveBytes==oldBytes);
 const char* target="/file-08192.txt";size_t at=0;
 Tab5HiddenFiles::calls=0;start=now();
 for(unsigned repeat=0;repeat<32;++repeat){for(at=0;at<baseline.rowCount();++at)if(baseline.row(at).path==target)break;assert(at+1==baseline.rowCount());}
 const auto baselineRevealNs=elapsed(start);const auto baselineHidden=Tab5HiddenFiles::calls;
 Tab5HiddenFiles::calls=0;start=now();
 for(unsigned repeat=0;repeat<32;++repeat){assert(current.rowIndex(target,at));assert(at+1==current.rowCount());}
 const auto currentRevealNs=elapsed(start);const auto currentHidden=Tab5HiddenFiles::calls;
 assert(currentHidden==0&&baselineHidden==32*baseline.rowCount());
 // Expansion after an idle plateau, cancelled work and refresh must re-arm.
 assert(current.collapse("/directory-0000"));assert(current.expand("/directory-0000"));assert(current.busy());finish(current);
 assert(current.refresh());assert(current.busy());current.cancel();assert(current.busy());finish(current);validateIndices(current);
 printf("{\"benchmark\":{\"hostOnly\":true,\"idleCalls\":200000,\"expandedDirectories\":512,\"baselineIdleNs\":%lld,\"currentIdleNs\":%lld,\"revealCalls\":32,\"visibleEntries\":8705,\"baselineRevealNs\":%lld,\"currentRevealNs\":%lld,\"baselineHiddenLookups\":%llu,\"currentHiddenLookups\":%llu}}\n",(long long)baselineIdleNs,(long long)currentIdleNs,(long long)baselineRevealNs,(long long)currentRevealNs,(unsigned long long)baselineHidden,(unsigned long long)currentHidden);
 current.clear();baseline.clear();assert(allocations.empty()&&activeHandles==0);
}
int main(){basic();sortingRefresh();large();cancellationAndRemoval();allocationFailures();depthAndPaths();performance();assert(peakHandles<=2);assert(liveBytes==0);std::printf("PASS: production lazy tree; expansion, collapse, depth 129, 8193 entries, 6 sorts, refresh, hidden files, cancellation, USB removal, allocation refusal, PSRAM only, max %d handles, peak %zu bytes.\n",peakHandles,peakBytes);}
