#include <cstdlib>
#include "Tab5AppCatalogue.cpp"
#include <cassert>
#include <vector>
#include <algorithm>
#include "Tab5AppRefresh.h"
fs::FS tab5FS;
static unsigned scriptNext=0;
static std::vector<Tab5PythonServices::AppInfo> scripts;
Tab5StorageUse::Tab5StorageUse(bool):held_(true){}
Tab5StorageUse::~Tab5StorageUse(){}
namespace Tab5UsbStorage{bool mounted(){return MockVolume::usb;}bool path(const char* p){return p&&!strncmp(p,"/USB",4)&&(p[4]=='/'||!p[4]);}}
namespace Tab5PythonServices{
bool beginAppScan(){scriptNext=0;return true;}
int nextApp(AppInfo& out){if(scriptNext<scripts.size()){out=scripts[scriptNext++];return 1;}return 2;}
void endAppScan(){}
}
static int finish(){for(unsigned n=0;n<1000;++n){int result=Tab5AppCatalogue::scanStep();if(result)return result;}assert(false);return -9;}
static void fresh(){Tab5AppCatalogue::endScan();assert(MockVolume::handles==0);MockVolume::entries.clear();MockVolume::usb=true;MockVolume::available=true;MockVolume::usbOpens=0;scripts.clear();Preferences::bytes.clear();assert(Tab5AppCatalogue::reset());assert(Tab5AppCatalogue::addBuiltin("files","Explorateur",1,1));}
static int find(const char* path){for(unsigned i=0;i<Tab5AppCatalogue::count();++i)if(!strcmp(Tab5AppCatalogue::at(i)->id,path))return int(i);return -1;}
int main(){using namespace Tab5AppCatalogue;using MockVolume::add;
 fresh();add("/Apps/zulu.elf");add("/Apps/alpha.app.elf");add("/Apps/.hidden.elf");add("/Apps/Quake.app.elf");add("/Apps/Duke3D.app.elf");add("/Apps/notes.txt");add("/Apps/demo.py");add("/Apps/suite",true);add("/Apps/suite/main.elf");add("/USB/Applications/usb.elf");
 Tab5PythonServices::AppInfo info;strcpy(info.path,"/Apps/demo.py");strcpy(info.title,"Demo");scripts.push_back(info);
 assert(beginScan(true)&&finish()==1&&MockVolume::handles==0);
 assert(count()==6&&find("/Apps/zulu.elf")>=0&&find("/Apps/suite/main.elf")>=0&&find("/USB/Applications/usb.elf")>=0);
 assert(find("/Apps/.hidden.elf")==-1&&find("/Apps/Quake.app.elf")==-1&&find("/Apps/notes.txt")==-1);
 unsigned order[MaxApps];assert(ordered(order,true)==6);assert(at(order[0])->kind==Kind::Builtin);
 assert(!strcmp(at(order[1])->title,"alpha")&&!strcmp(at(order[2])->title,"suite")&&!strcmp(at(order[3])->title,"usb")&&!strcmp(at(order[4])->title,"zulu"));
 assert(at(order[5])->kind==Kind::Script);
 const unsigned zulu=find("/Apps/zulu.elf"),demo=find("/Apps/demo.py");
 assert(!move(demo,-1));assert(move(zulu,-1));ordered(order,true);assert(!strcmp(at(order[3])->title,"zulu"));
 assert(pin(zulu,false)&&ordered(order,true)==5&&saveChoices());assert(loadChoices()&&!pinned(zulu));
 assert(beginScan(true));scanStep();endScan();assert(MockVolume::handles==0);
 // English shipped names and legacy names both preserve catalogue IDs and UI labels.
 fresh();
 const char* paths[]={"/MicroPython/libraries.py","/MicroPython/bibliotheques.py","/MicroPython/example.py","/MicroPython/exemple.py"};
 for(const char* path:paths){Tab5PythonServices::AppInfo app{};strcpy(app.path,path);strcpy(app.title,"raw filename");scripts.push_back(app);}
 assert(beginScan(true)&&finish()==1&&count()==5);
 for(unsigned i=0;i<4;++i){int index=find(paths[i]);assert(index>=0);assert(!strcmp(at(index)->title,i<2?"Modules et projets":"Exemple"));}
 assert(MockVolume::handles==0);
 puts("PASS: English and legacy script names keep original IDs and localized display names");
 // USB-only discovery works when the microSD is deliberately unavailable.
 fresh();add("/Apps/not-mounted.elf");add("/USB/Apps/portable.elf");assert(beginScan(false)&&finish()==1);assert(count()==2&&find("/Apps/not-mounted.elf")==-1&&MockVolume::handles==0);
 // An absent USB disk is never opened.
 fresh();MockVolume::usb=false;add("/USB/Apps/unavailable.elf");add("/Apps/local.elf");assert(beginScan(true)&&finish()==1&&MockVolume::usbOpens==0&&count()==2);
 // A bounded catalogue stops with all directory references released.
 fresh();for(unsigned i=0;i<100;++i){char path[64];snprintf(path,sizeof(path),"/Apps/app-%03u.elf",i);add(path);}assert(beginScan(true)&&finish()==2&&count()==MaxApps&&MockVolume::handles==0);
 // A scan cancelled while holding a directory leaves no live storage handle.
 fresh();add("/Apps/test.elf");assert(beginScan(true));while(!MockVolume::handles)assert(scanStep()==0);endScan();assert(MockVolume::handles==0);

 // A scan never exposes a partial library and an unchanged scan publishes no
 // revision: existing card indices, focus and scroll are retained by the UI.
 fresh();add("/Apps/old.elf");add("/Apps/keep.elf");assert(beginScan(true)&&finish()==1);
 const uint32_t initial=revision();const unsigned keep=find("/Apps/keep.elf");const App* pointer=at(keep);
 assert(pin(keep,false)&&saveChoices());
 assert(beginScan(true));assert(count()==3&&revision()==initial&&at(keep)==pointer);
 assert(finish()==1&&revision()==initial&&at(keep)==pointer);
 std::reverse(MockVolume::entries["/Apps"].children.begin(),MockVolume::entries["/Apps"].children.end());
 assert(beginScan(true)&&finish()==1&&revision()==initial&&at(keep)==pointer&&!pinned(keep));
 add("/Apps/new.elf");assert(beginScan(true));assert(find("/Apps/new.elf")==-1&&count()==3);
 while(scanStep()==0)assert(count()==3&&revision()==initial);
 assert(count()==4&&revision()==initial+1&&find("/Apps/new.elf")>=0&&!pinned(find("/Apps/keep.elf")));
 auto& children=MockVolume::entries["/Apps"].children;children.erase(std::remove(children.begin(),children.end(),"/Apps/old.elf"),children.end());MockVolume::entries.erase("/Apps/old.elf");
 assert(beginScan(true)&&finish()==1&&find("/Apps/old.elf")==-1&&count()==3);
 const uint32_t removed=revision();add("/Apps/later.elf");assert(beginScan(true));scanStep();endScan();
 assert(revision()==removed&&count()==3&&find("/Apps/later.elf")==-1&&MockVolume::handles==0);
 // I/O failure leaves the last complete catalogue available; a retry converges.
 MockVolume::failOpen=true;assert(beginScan(true)&&finish()==-1);assert(revision()==removed&&count()==3&&MockVolume::handles==0);MockVolume::failOpen=false;
 assert(beginScan(true)&&finish()==1&&find("/Apps/later.elf")>=0);
 const uint32_t intact=revision();const unsigned intactCount=count();
 heap_caps_free(staging);staging=nullptr;MockHeap::fail=true;
 assert(!beginScan(true)&&revision()==intact&&count()==intactCount&&MockVolume::handles==0);MockHeap::fail=false;
 assert(beginScan(true)&&finish()==1&&revision()==intact);
 const unsigned allocated=MockHeap::allocations;
 for(unsigned i=0;i<100;++i)assert(beginScan(true)&&finish()==1&&revision()==intact);
 assert(MockHeap::allocations==allocated);
 MockVolume::usb=false;assert(!beginScan(false));assert(count()==1&&MockVolume::handles==0);
 // Scheduling is bounded, debounces notification bursts, catches out-of-band
 // writes, and continues correctly across millis() wraparound.
 Tab5AppRefresh::Schedule q;q.observe(100,1);q.started(100);q.finished(300);
 assert(!q.due(5299)&&q.due(5300));q.started(5300);q.observe(5400,2);q.finished(5500);assert(!q.due(7299)&&q.due(7300));
 q.started(7300);q.observe(7400,3);q.observe(7800,4);q.finished(7900);assert(!q.due(9299)&&q.due(9300));
 q.started(0xfffffff0u);q.finished(0xfffffff5u);q.observe(20,5);assert(!q.due(600)&&q.due(6000));
 puts("PASS: transactional dynamic add/remove, no-op/enumeration stability, cancellation/error rollback, persistent choices, periodic/debounced wrap-safe scheduling");
 puts("PASS: actual catalogue SD/USB scan, raw ELF, app folders, exclusions, deduplication, grouping/order, persistence and handle cleanup");
}
