#include "Platform.h"
#include "Tab5AtomicFile.h"
#include <cassert>
#include <cstdio>
constexpr size_t MaxPath=260;
struct Context {unsigned abi=4;} instance;
Context* context=&instance;
bool worker(){return true;}
void vTaskDelay(unsigned){}
#include "production.inc"
const char* path="/Saves/GB/test.sav";
const char* backup="/Saves/GB/test.sav.old";
const char* temporary="/Saves/GB/test.sav.tmp";
const char* replacement="NEW SAVE CONTENT";
void put(const char* name,const char* value){auto n=std::make_shared<Node>();n->bytes=value;files[name]=n;}
std::string get(const char* name){auto it=files.find(name);return it==files.end()?std::string():it->second->bytes;}
void reset(bool recover=false){
 files.clear();failOperation=-1;operationCount=0;statErrorPath.clear();openErrorPath.clear();statErrorAfterOpen=openFailed=flushFault=closeFault=false;
 tab5FS.mkdir("/Saves");tab5FS.mkdir("/Saves/GB");
 if(recover)put(backup,"ORIGINAL");else {put(path,"ORIGINAL");put(backup,"OLDER");}
 put(temporary,"STALE TEMP");operationCount=0;
}
int save(){return appSave(path,replacement,std::strlen(replacement));}
int main(int argc,char** argv){
 const bool baseline=argc>1&&!std::strcmp(argv[1],"baseline");
 reset();statErrorPath=path;
 int result=save();
 if(baseline){assert(result==0&&get(path)==replacement&&get(backup).empty());reset();flushFault=true;assert(save()==0&&get(path)==replacement);reset();closeFault=true;assert(save()==0&&get(path)==replacement);std::puts("baseline confirmed: stat EIO was treated as absence and erased recovery backup; flush/close EIO without write-error flag were also reported as successful save");return 0;}
 assert(result==-1&&get(path)=="ORIGINAL"&&get(backup)=="OLDER"&&get(temporary)=="STALE TEMP");
 reset(true);statErrorPath=backup;assert(save()==-1&&get(backup)=="ORIGINAL"&&get(temporary)=="STALE TEMP");
 reset();statErrorPath=temporary;assert(save()==-1&&get(path)=="ORIGINAL"&&get(backup)=="OLDER");
 reset();flushFault=true;assert(save()==-1&&get(path)=="ORIGINAL"&&get(backup)=="OLDER");
 reset();closeFault=true;assert(save()==-1&&get(path)=="ORIGINAL"&&get(backup)=="OLDER");
 unsigned cases=5;
 for(bool recovery:{false,true})for(int failure=0;failure<40;++failure){
  reset(recovery);failOperation=failure;result=save();
  if(result==0)assert(get(path)==replacement);
  else assert(get(path)=="ORIGINAL"||get(backup)=="ORIGINAL");
  ++cases;
 }
 reset();assert(appSave("/Pictures/a.sav",replacement,std::strlen(replacement))==-1);
 assert(appSave(path,replacement,0)==-1);assert(appSave(path,replacement,2*1024*1024+1)==-1);
 reset(true);assert(save()==0&&get(path)==replacement&&get(backup).empty());
 std::printf("%u save fault cases: stat/flush/close EIO rejected even without write-error flag, every failed replacement retains original or .old, backup recovery checked, successful save preserves existing cleanup behavior\n",cases);
}
