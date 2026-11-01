#include "Tab5ExplorerTree.h"
#include <chrono>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <algorithm>

static size_t checks=0;
#define CHECK(expr) do { ++checks; if(!(expr)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);std::abort();} } while(false)
static std::shared_ptr<Node> dir(const std::string& path){auto n=std::make_shared<Node>();n->name=path;n->directory=true;tab5FS.nodes[path]=n;return n;}
static std::shared_ptr<Node> add(const std::string& parent,const std::string& name,bool folder=false,uint64_t bytes=0,int64_t modified=1234){
 auto n=std::make_shared<Node>();n->name=name;n->directory=folder;n->bytes=bytes;n->modified=modified;
 const auto path=parent+(parent=="/"?"":"/")+name;tab5FS.nodes[path]=n;tab5FS.nodes.at(parent)->children.push_back(n);return n;
}
static void finish(Tab5ExplorerTree& tree){
 unsigned calls=0;while(tree.busy()){const int before=reads;tree.service();CHECK(reads-before<=16);CHECK(++calls<150000);}
}
static void finishAll(Tab5ExplorerTree& tree){
 unsigned calls=0;while(tree.busy()||tree.probing()){const int before=reads;tree.service();CHECK(reads-before<=16);CHECK(++calls<150000);}CHECK(activeHandles==0);
}
static size_t index(Tab5ExplorerTree& tree,const std::string& path){size_t at=tree.rowCount();if(tree.rowIndex(path.c_str(),at))CHECK(tree.row(at).path==path);return at;}
static void validateIndices(Tab5ExplorerTree& tree){
 std::set<std::string> seen;
 for(size_t i=0;i<tree.rowCount();++i){auto row=tree.row(i);size_t actual=1234567;CHECK(row.directory);CHECK(seen.insert(row.path).second);CHECK(tree.rowIndex(row.path.c_str(),actual));CHECK(actual==i);}
 size_t unchanged=42;CHECK(!tree.rowIndex("/missing/never",unchanged)&&unchanged==42);CHECK(!tree.rowIndex("relative",unchanged)&&unchanged==42);
}
static void reset(){
 CHECK(activeHandles==0);CHECK(allocations.empty());tab5FS.nodes.clear();openedPaths.clear();Tab5Files::showHidden=false;Tab5HiddenFiles::hiddenPaths.clear();Tab5UsbStorage::isMounted=false;Tab5FileSort::selected=Tab5FileSort::Order::NameAscending;failAfter=-1;
 filesystemOpens=reads=fileSizeReads=fileDateReads=directorySizeReads=directoryDateReads=0;
}
static void basic(){
 reset();dir("/");add("/","z.txt");add("/","B",true);add("/","A",true);add("/A","inside.txt");add("/A","nested",true);add("/A/nested","deep.txt");add("/B","other.txt");
 Tab5ExplorerTree tree;CHECK(tree.begin("/"));CHECK(!tree.volumes());finish(tree);CHECK(tree.rowCount()==2&&tree.row(0).name=="A");CHECK(fileSizeReads==0&&fileDateReads==0);
 CHECK(openedPaths==std::vector<std::string>{"/"});const std::string root=tree.rootPath();
 tree.clearDirty();CHECK(tree.expand("/A")&&tree.dirty());finish(tree);CHECK(tree.rowCount()==3&&tree.row(1).path=="/A/nested"&&tree.row(1).depth==1);
 CHECK(tree.expand("/A/nested"));CHECK(tree.expand("/B"));finish(tree);CHECK(tree.rowCount()==3);
 const size_t expandedBytes=liveBytes;const int opens=filesystemOpens;CHECK(tree.collapse("/A"));CHECK(tree.rowCount()==2&&tree.expanded("/B")&&!tree.expanded("/A/nested"));CHECK(liveBytes<expandedBytes);CHECK(filesystemOpens==opens&&tree.rootPath()==root);
 CHECK(!tree.expand("/z.txt")&&!tree.expand("/A/nested"));CHECK(tree.expand("/A"));finish(tree);CHECK(!tree.expanded("/A/nested"));validateIndices(tree);
 CHECK(tree.row(tree.rowCount()).path.empty());tree.clear();CHECK(allocations.empty());
}
static void volumes(){
 reset();dir("/");add("/","Documents",true);add("/Documents","Nested",true);add("/","root.txt");dir("/USB");add("/USB","Portable",true);Tab5UsbStorage::isMounted=true;
 Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));CHECK(tree.volumes());finishAll(tree);CHECK(tree.expanded("/"));CHECK(!tree.expanded("/USB"));
 CHECK(tree.rowCount()==3);CHECK(tree.row(0).path=="/"&&tree.row(0).volume&&tree.row(0).depth==0);CHECK(tree.row(1).path=="/Documents"&&!tree.row(1).volume&&tree.row(1).depth==1);CHECK(tree.row(2).path=="/USB"&&tree.row(2).volume&&tree.row(2).depth==0);validateIndices(tree);
 CHECK(tree.expand("/Documents"));finishAll(tree);CHECK(tree.row(index(tree,"/Documents/Nested")).depth==2);CHECK(tree.expand("/USB"));finishAll(tree);CHECK(tree.row(index(tree,"/USB/Portable")).depth==1);validateIndices(tree);
 const int noChangeOpens=filesystemOpens;tree.syncVolumes(true);tree.service();CHECK(filesystemOpens==noChangeOpens&&tree.expanded("/Documents"));
 CHECK(tree.collapse("/"));CHECK(!tree.expanded("/")&&tree.expanded("/USB"));CHECK(index(tree,"/Documents")==tree.rowCount());CHECK(tree.rowCount()==3);CHECK(tree.expand("/"));finishAll(tree);CHECK(tree.expanded("/USB"));
 CHECK(tree.expand("/Documents"));finishAll(tree);Tab5UsbStorage::isMounted=false;tree.syncVolumes(true);finishAll(tree);CHECK(index(tree,"/USB")==tree.rowCount()&&!tree.expanded("/USB"));CHECK(tree.expanded("/Documents"));
 Tab5UsbStorage::isMounted=true;tree.syncVolumes(true);finishAll(tree);CHECK(index(tree,"/USB")<tree.rowCount()&&!tree.expanded("/USB"));CHECK(tree.expanded("/Documents"));
 tree.syncVolumes(false);finishAll(tree);CHECK(index(tree,"/")==tree.rowCount()&&index(tree,"/Documents")==tree.rowCount());CHECK(tree.rowCount()==1&&tree.row(0).path=="/USB");
 tree.clear();CHECK(allocations.empty());CHECK(tree.beginVolumes(false));finishAll(tree);CHECK(tree.expanded("/USB")&&tree.rowCount()==2);CHECK(tree.row(0).path=="/USB");
 Tab5UsbStorage::isMounted=false;tree.service();finishAll(tree);CHECK(tree.rowCount()==0&&activeHandles==0);tree.clear();CHECK(allocations.empty());
 CHECK(tree.beginVolumes(false));finishAll(tree);CHECK(tree.rowCount()==0);tree.clear();CHECK(allocations.empty());
}
static void sortingRefresh(){
 reset();dir("/");add("/","z",true,900,60);add("/","a",true,200,20);add("/","file",false,2000,80);add("/a","x",true,100,2);add("/a","y",true,200,1);add("/z","zchild",true);
 Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));finishAll(tree);CHECK(tree.expand("/a")&&tree.expand("/z"));finishAll(tree);
 for(unsigned order=0;order<6;++order){Tab5FileSort::selected=static_cast<Tab5FileSort::Order>(order);tree.service();CHECK(tree.rowCount()==6);CHECK(tree.row(0).path=="/");CHECK(tree.row(index(tree,"/a/x")).depth==2);CHECK(tree.row(index(tree,"/z/zchild")).depth==2);CHECK(tree.expanded("/a")&&tree.expanded("/z"));validateIndices(tree);}
 Tab5FileSort::selected=Tab5FileSort::Order::NameAscending;CHECK(tree.refresh());finishAll(tree);CHECK(tree.expanded("/a")&&tree.expanded("/z")&&tree.rowCount()==6);
 tab5FS.nodes["/"]->children.erase(tab5FS.nodes["/"]->children.begin());CHECK(tree.refresh());finishAll(tree);CHECK(!tree.expanded("/z")&&tree.expanded("/a")&&tree.rowCount()==4);validateIndices(tree);
 tree.clear();CHECK(allocations.empty());
}
static void large(){
 reset();dir("/large");for(unsigned i=0;i<8193;++i)add("/large","folder-"+std::to_string(i),true,i,i);for(unsigned i=0;i<1000;++i)add("/large","ignored-file-"+std::to_string(i),false,i,i);
 add("/large",".secret",true);add("/large",".",true);add("/large","..",true);Tab5HiddenFiles::hiddenPaths.insert("/large/folder-100");
 Tab5ExplorerTree tree;CHECK(tree.begin("/large"));finishAll(tree);CHECK(tree.rowCount()==8192);CHECK(fileSizeReads==0&&fileDateReads==0);
 std::set<std::string> seen;const int opens=filesystemOpens;for(size_t page=0;page<tree.rowCount();page+=128)for(size_t i=page;i<std::min(page+128,tree.rowCount());++i){auto row=tree.row(i);CHECK(row.depth==0&&!row.hidden&&row.directory);CHECK(seen.insert(row.path).second);}
 CHECK(filesystemOpens==opens);validateIndices(tree);Tab5Files::showHidden=true;CHECK(tree.refresh());finishAll(tree);CHECK(tree.rowCount()==8194);CHECK(tree.row(index(tree,"/large/.secret")).hidden);tree.clear();CHECK(allocations.empty());
}
static void probing(){
 reset();dir("/");add("/","FilesOnly",true);add("/","ContainsFolder",true);add("/","HiddenOnly",true);add("/","Deferred",true);
 for(unsigned i=0;i<100;++i)add("/FilesOnly","f"+std::to_string(i));add("/ContainsFolder","prefix.txt");add("/ContainsFolder","Child",true);for(unsigned i=0;i<200;++i)add("/ContainsFolder","after"+std::to_string(i));add("/HiddenOnly",".secret",true);add("/HiddenOnly","ignored.txt");add("/Deferred","Later",true);
 Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));finishAll(tree);tree.clearDirty();tree.clearHintsDirty();CHECK(!tree.hasSubdirectories("/FilesOnly")&&!tree.hasSubdirectories("/ContainsFolder"));
 const int opens=filesystemOpens,beginReads=reads;tree.requestChildProbe("/ContainsFolder");CHECK(tree.probing());CHECK(!tree.busy());
 while(tree.probing()){const int before=reads;tree.service();CHECK(reads-before<=8);CHECK(!tree.busy());}
 CHECK(filesystemOpens==opens+1);CHECK(reads-beginReads==2);CHECK(tree.hasSubdirectories("/ContainsFolder"));CHECK(tree.hintsDirty());CHECK(!tree.dirty());
 const int cachedReads=reads,cachedOpens=filesystemOpens;tree.clearHintsDirty();for(unsigned i=0;i<100;++i){tree.requestChildProbe("/ContainsFolder");tree.service();}CHECK(reads==cachedReads&&filesystemOpens==cachedOpens&&!tree.hintsDirty());
 tree.requestChildProbe("/FilesOnly");const int firstReads=reads;tree.service();CHECK(reads-firstReads<=8&&tree.probing());finishAll(tree);CHECK(!tree.hasSubdirectories("/FilesOnly"));CHECK(index(tree,"/FilesOnly")<tree.rowCount());
 tree.requestChildProbe("/HiddenOnly");finishAll(tree);CHECK(!tree.hasSubdirectories("/HiddenOnly"));CHECK(std::count(openedPaths.begin(),openedPaths.end(),"/Deferred")==0);CHECK(std::count(openedPaths.begin(),openedPaths.end(),"/ContainsFolder/Child")==0);CHECK(fileSizeReads==0&&fileDateReads==0);
 tree.requestChildProbe("/FilesOnly");finishAll(tree);CHECK(std::count(openedPaths.begin(),openedPaths.end(),"/FilesOnly")==1);
 add("/FilesOnly","NewChild",true);CHECK(tree.refresh());finishAll(tree);tree.requestChildProbe("/FilesOnly");finishAll(tree);CHECK(tree.hasSubdirectories("/FilesOnly"));CHECK(std::count(openedPaths.begin(),openedPaths.end(),"/FilesOnly")==2);
 Tab5Files::showHidden=true;CHECK(tree.refresh());finishAll(tree);tree.requestChildProbe("/HiddenOnly");finishAll(tree);CHECK(tree.hasSubdirectories("/HiddenOnly"));
 tree.clear();CHECK(allocations.empty()&&activeHandles==0);
}
static void cancellationAndRemoval(){
 reset();dir("/");add("/","A",true);add("/","B",true);add("/B","keep",true);for(unsigned i=0;i<60;++i)add("/A","f"+std::to_string(i),true);
 Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));finishAll(tree);CHECK(tree.expand("/B"));finishAll(tree);CHECK(tree.expand("/A"));tree.service();CHECK(activeHandles==1);CHECK(tree.collapse("/A"));CHECK(activeHandles==0&&tree.expanded("/B")&&tree.rowCount()==4);
 CHECK(tree.expand("/A"));tree.service();tree.cancel();CHECK(activeHandles==0&&tree.expanded("/A")&&tree.expanded("/B"));finishAll(tree);CHECK(tree.rowCount()==64);
 Tab5UsbStorage::isMounted=true;dir("/USB");add("/USB","folder",true);for(unsigned i=0;i<60;++i)add("/USB/folder","u"+std::to_string(i),true);tree.syncVolumes(true);CHECK(tree.expand("/USB"));finishAll(tree);
 CHECK(tree.expand("/USB/folder"));tree.service();CHECK(activeHandles==1);Tab5UsbStorage::isMounted=false;tree.service();finishAll(tree);CHECK(activeHandles==0&&!tree.expanded("/USB")&&index(tree,"/USB")==tree.rowCount()&&tree.expanded("/B"));
 tree.requestChildProbe("/A/f0");tree.cancel();CHECK(activeHandles==0);finishAll(tree);tree.clear();CHECK(allocations.empty());
 CHECK(!tree.begin("/missing")&&tree.failed()&&!tree.busy());tree.clear();CHECK(allocations.empty());
}
static void volumeScanIsolation(){
 reset();dir("/");add("/","Local",true);dir("/USB");for(unsigned i=0;i<80;++i)add("/USB","Portable"+std::to_string(i),true);for(unsigned i=0;i<100;++i)add("/USB/Portable0","file"+std::to_string(i));Tab5UsbStorage::isMounted=true;
 Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));finishAll(tree);CHECK(tree.expand("/USB"));tree.service();CHECK(tree.busy()&&activeHandles==1);
 CHECK(tree.collapse("/"));CHECK(tree.busy());finishAll(tree);CHECK(tree.expanded("/USB")&&tree.rowCount()==82);CHECK(index(tree,"/USB/Portable79")<tree.rowCount());
 CHECK(tree.expand("/"));finishAll(tree);tree.requestChildProbe("/USB/Portable0");tree.service();CHECK(tree.probing()&&activeHandles==1);CHECK(tree.collapse("/"));CHECK(tree.probing()&&activeHandles==1);finishAll(tree);CHECK(!tree.hasSubdirectories("/USB/Portable0"));tree.clear();CHECK(allocations.empty());
}
static void retainedMetadataAndProbeQueue(){
 reset();dir("/");add("/","A",true);add("/A","B",true);add("/A/B","C",true);add("/A","D",true);for(unsigned i=0;i<80;++i)add("/A/D","file"+std::to_string(i));
 Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));finishAll(tree);CHECK(tree.expand("/A"));finishAll(tree);CHECK(tree.expand("/A/B"));finishAll(tree);
 tree.requestChildProbe("/A/D");tree.service();CHECK(tree.probing()&&activeHandles==1);tree.cancel();CHECK(activeHandles==0);tree.requestChildProbe("/A/D");finishAll(tree);CHECK(!tree.hasSubdirectories("/A/D"));
 const int beforeReleaseOpens=filesystemOpens;const size_t beforeReleaseBytes=liveBytes;const size_t beforeReleaseRows=tree.rowCount();tree.releaseMetadata();CHECK(filesystemOpens==beforeReleaseOpens&&activeHandles==0);CHECK(tree.expanded("/")&&tree.expanded("/A")&&tree.expanded("/A/B")&&tree.busy());CHECK(liveBytes<beforeReleaseBytes);finishAll(tree);CHECK(tree.rowCount()==beforeReleaseRows);validateIndices(tree);
 tree.requestChildProbe("/A/D");tree.service();CHECK(tree.probing());CHECK(tree.collapse("/A"));CHECK(activeHandles==0);const int collapsedOpens=filesystemOpens,collapsedReads=reads;finishAll(tree);CHECK(filesystemOpens==collapsedOpens&&reads==collapsedReads);
 CHECK(tree.expand("/A"));finishAll(tree);tree.requestChildProbe("/A/D");tree.requestChildProbe("/A/D");finishAll(tree);CHECK(std::count(openedPaths.begin(),openedPaths.end(),"/A/D")==4);tree.clear();CHECK(allocations.empty());
}
static void volumeAllocationFailureDuringScan(){
 reset();dir("/");for(unsigned i=0;i<40;++i)add("/","local"+std::to_string(i),true);dir("/USB");add("/USB","Portable",true);
 for(long failure=0;failure<5;++failure){
  Tab5UsbStorage::isMounted=false;Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));CHECK(tree.busy());Tab5UsbStorage::isMounted=true;failAfter=failure;tree.syncVolumes(true);tree.service();failAfter=-1;finishAll(tree);tree.clear();CHECK(allocations.empty()&&activeHandles==0);
 }
}
static void expansionTakesOverProbe(){
 for(unsigned action=0;action<3;++action){
  reset();dir("/");add("/","Target",true);for(unsigned i=0;i<100;++i)add("/Target","file"+std::to_string(i));
  Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));finishAll(tree);tree.requestChildProbe("/Target");tree.service();CHECK(tree.probing()&&activeHandles==1);
  CHECK(tree.expand("/Target"));tree.service();CHECK(tree.busy()&&activeHandles==1);CHECK(std::count(openedPaths.begin(),openedPaths.end(),"/Target")==2);
  if(action==1){CHECK(tree.collapse("/Target"));CHECK(activeHandles==0);}
  if(action==2){tree.cancel();CHECK(activeHandles==0);}
  finishAll(tree);CHECK(!tree.hasSubdirectories("/Target"));CHECK(!tree.probing());CHECK(tree.expanded("/Target")==bool(action!=1));CHECK(std::count(openedPaths.begin(),openedPaths.end(),"/Target")==int(action?3:2));
  tree.clear();CHECK(allocations.empty()&&activeHandles==0);
 }
}
static void allocationFailures(){
 reset();dir("/");add("/","folder",true);for(unsigned i=0;i<150;++i)add("/folder","child-"+std::to_string(i),true);
 for(long failure=0;failure<28;++failure){Tab5ExplorerTree tree;failAfter=failure;tree.beginVolumes(true);finish(tree);tree.requestChildProbe("/folder");finishAll(tree);if(!tree.failed()){tree.expand("/folder");finishAll(tree);}tree.clear();CHECK(allocations.empty()&&activeHandles==0);failAfter=-1;}
 Tab5ExplorerTree tree;CHECK(tree.begin("/"));finishAll(tree);const auto bytes=liveBytes;failAfter=0;CHECK(!tree.expand("/folder")&&tree.rowCount()==1&&liveBytes==bytes);tree.requestChildProbe("/folder");tree.service();failAfter=-1;CHECK(tree.expand("/folder"));finishAll(tree);CHECK(tree.rowCount()==151);tree.clear();CHECK(allocations.empty());
}
static void depthAndPaths(){
 reset();dir("/");std::string path="/";for(unsigned depth=0;depth<129;++depth){add(path,"a",true);path+=(path=="/"?"":"/");path+="a";}add(path,"z",true);
 Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));finishAll(tree);path="/";
 for(unsigned depth=0;depth<129;++depth){path+=(path=="/"?"":"/");path+="a";CHECK(tree.expand(path.c_str()));finishAll(tree);CHECK(tree.row(depth+1).depth==depth+1);}
 validateIndices(tree);CHECK(tree.rowCount()==130&&path.size()==258);CHECK(tree.collapse("/a")&&tree.rowCount()==2);tree.clear();CHECK(allocations.empty());
 CHECK(!tree.begin(std::string(260,'/').c_str()));tree.clear();CHECK(allocations.empty());
}
static void idleAndLookup(){
 reset();dir("/");for(unsigned i=0;i<512;++i){char name[40];snprintf(name,sizeof(name),"directory-%04u",i);add("/",name,true);}for(unsigned i=0;i<8193;++i)add("/","ignored-file-"+std::to_string(i));
 Tab5ExplorerTree tree;CHECK(tree.beginVolumes(true));finishAll(tree);for(unsigned i=0;i<512;++i){char path[40];snprintf(path,sizeof(path),"/directory-%04u",i);CHECK(tree.expand(path));}finishAll(tree);CHECK(tree.rowCount()==513);validateIndices(tree);
 const int oldReads=reads,oldOpens=filesystemOpens;const size_t oldBytes=liveBytes;
 tree.clearDirty();tree.clearHintsDirty();tree.cancel();CHECK(!tree.dirty()&&!tree.hintsDirty()&&!tree.busy()&&!tree.probing());CHECK(tree.expanded("/")&&tree.expanded("/directory-0000")&&tree.expanded("/directory-0511"));CHECK(tree.rowCount()==513);validateIndices(tree);tree.service();CHECK(reads==oldReads&&filesystemOpens==oldOpens&&liveBytes==oldBytes);CHECK(!tree.dirty()&&!tree.hintsDirty());
 const auto before=std::chrono::steady_clock::now();
 for(unsigned i=0;i<200000;++i){CHECK(!tree.busy()&&!tree.probing());tree.service();}
 const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-before).count();CHECK(reads==oldReads&&filesystemOpens==oldOpens&&liveBytes==oldBytes);
 Tab5HiddenFiles::calls=0;size_t at=0;for(unsigned repeat=0;repeat<32;++repeat){CHECK(tree.rowIndex("/directory-0511",at));CHECK(at+1==tree.rowCount());}CHECK(Tab5HiddenFiles::calls==0);CHECK(fileSizeReads==0&&fileDateReads==0);
 std::printf("{\"benchmark\":{\"hostOnly\":true,\"idleCalls\":200000,\"expandedDirectories\":512,\"currentIdleNs\":%lld,\"revealCalls\":32,\"visibleEntries\":513,\"ignoredFileEntries\":8193,\"idleReads\":0,\"idleOpens\":0,\"lookupHiddenCalls\":0}}\n",(long long)elapsed);
 tree.clear();CHECK(allocations.empty()&&activeHandles==0);
}
int main(){basic();volumes();sortingRefresh();large();probing();cancellationAndRemoval();volumeScanIsolation();retainedMetadataAndProbeQueue();volumeAllocationFailureDuringScan();expansionTakesOverProbe();allocationFailures();depthAndPaths();idleAndLookup();CHECK(peakHandles<=3);CHECK(liveBytes==0);std::printf("{\"checks\":%zu,\"peakHandles\":%d,\"peakPsramBytes\":%zu}\n",checks,peakHandles,peakBytes);std::printf("PASS: exact production folders-only tree; mounted volume roots, session branches, depth129,8193 directories,6sorts; bounded cached child probes,hidden filters,refresh,cancellation,USB removal,PSRAM refusal,zero file metadata reads,zero idle IO.\n");}
