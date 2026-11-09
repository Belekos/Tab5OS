#include "Tab5AppDirectory.h"
#include <algorithm>
#include <atomic>
#include <assert.h>
#include <errno.h>
#include <map>
#include <memory>
#include <stdio.h>
#include <string>
#include <vector>

struct Node {bool directory=false;size_t bytes=0;std::vector<std::string> children;};
static std::map<std::string,Node> nodes;
static unsigned live=0,peak=0,opens=0;
static bool iterationError=false,onWorker=true;
struct State {
 std::string path;size_t offset=0,index=0;bool opened=true;
 explicit State(const std::string& value):path(value){peak=std::max(peak,++live);}
 ~State(){if(opened)--live;}
};
class File {
 std::shared_ptr<State> state;
 public:
 File()=default;
 explicit File(const std::string& path){if(nodes.count(path))state=std::make_shared<State>(path);}
 explicit operator bool()const{return state&&state->opened;}
 bool isDirectory()const{return *this&&nodes.at(state->path).directory;}
 void close(){if(*this){state->opened=false;--live;}state.reset();}
 const char* name()const{return state->path.c_str();}
 size_t size()const{return *this?nodes.at(state->path).bytes:0;}
 size_t position()const{return state->offset;}
 bool seek(size_t offset){state->offset=offset;return true;}
 size_t read(uint8_t* output,size_t bytes){memset(output,0x42,bytes);state->offset+=bytes;return bytes;}
 File openNextFile(int){
  if(iterationError){errno=EIO;return {};}
  auto& children=nodes.at(state->path).children;
  return state->index<children.size()?File(children[state->index++]):File();
 }
};
struct FS {File open(const char* path,int){++opens;return File(path);}} tab5FS;
constexpr int FILE_READ=0;
constexpr size_t MaxPath=260;
struct Context {File files[TAB5_APP_MAX_FILES];unsigned directoryEntries[TAB5_APP_MAX_FILES]{};char argument[512]{};} instance;
Context* context=&instance;
std::atomic<bool> cancelled{false};
bool worker(){return onWorker;}
bool cooperate(){return worker();}
struct ImageWriter {static bool valid(const char*){return false;}static bool recover(FS&,const char*){return true;}};
const char* sourcePath="/Applications/DOS.app.elf";
void tab5StorageFault(const char*,const char*,int){assert(false);}
// Compiled directly from the production implementations by run_tests.py.
#include "broker_callbacks.inc"

void node(const std::string& path,bool directory,size_t bytes=0){nodes[path]={directory,bytes,{}};}
void child(const std::string& parent,const std::string& name,bool directory=false,size_t bytes=1){std::string path=parent+"/"+name;node(path,directory,bytes);nodes[parent].children.push_back(path);}
void reset(){for(auto& file:context->files)file.close();assert(live==0);context->argument[0]=0;cancelled=false;iterationError=false;onWorker=true;}
int main(){
 node("/DOS",true);child("/DOS","FREEDOS.IMG",false,1474560);child("/DOS","GAMES",true);child("/DOS/GAMES","TEST.COM",false,64);
 node("/Programs",true);child("/Programs","START.EXE",false,100);child("/Programs","DATA.DAT",false,200);
 child("/Programs",".hidden",false,3);child("/Programs","nested",true);child("/Programs/nested","MORE.DAT",false,4);
 node("/Unrelated",true);node("/",true);
 tab5_app_directory_entry entry{};entry.size=sizeof(entry);
 int handle=appDirectoryOpen("/DOS");assert(handle>0);
 assert(appDirectoryNext(handle,&entry)==1&&!strcmp(entry.name,"FREEDOS.IMG")&&entry.file_size==1474560);
 assert(appDirectoryNext(handle,&entry)==1&&!strcmp(entry.name,"GAMES")&&entry.flags==1);
 assert(appDirectoryNext(handle,&entry)==0);assert(appClose(handle)==0&&live==0);
 assert(appDirectoryOpen("/Programs")==-1&&appDirectoryOpen("/DOS/FREEDOS.IMG")==-1);
 assert(appDirectoryOpen("/DOS/../Programs")==-1&&appDirectoryOpen("/DOS2")==-1);
 strcpy(context->argument,"/Programs/START.EXE");handle=appDirectoryOpen("/Programs");assert(handle>0);
 const char* names[]={"START.EXE","DATA.DAT",".hidden","nested"};
 for(auto name:names){entry.size=sizeof(entry);assert(appDirectoryNext(handle,&entry)==1&&!strcmp(entry.name,name));}
 assert(appDirectoryNext(handle,&entry)==0&&appClose(handle)==0);
 assert(appDirectoryOpen("/Unrelated")==-1);
 handle=appDirectoryOpen("/Programs/nested");assert(handle>0&&appClose(handle)==0);
 strcpy(context->argument,"/Programs/nested");assert(appDirectoryOpen("/Programs")==-1);
 handle=appDirectoryOpen("/Programs/nested");assert(handle>0&&appClose(handle)==0);
 strcpy(context->argument,"/Programs/MISSING.EXE");assert(appDirectoryOpen("/Programs")==-1);
 reset();
 // Eight slots are shared by directories and ordinary read-only files.
 for(unsigned i=0;i<8;++i)assert((i%2?appOpen("/DOS/FREEDOS.IMG"):appDirectoryOpen("/DOS"))==int(i+1));
 assert(live==8&&appOpen("/DOS/FREEDOS.IMG")==-1&&appDirectoryOpen("/DOS")==-1);
 uint8_t bytes[4];assert(appRead(1,bytes,sizeof(bytes))==-1&&appSeek(1,0)==-1);
 assert(appDirectoryNext(2,&entry)==-1);assert(appRead(2,bytes,sizeof(bytes))==4&&bytes[0]==0x42);
 assert(appClose(1)==0&&appDirectoryOpen("/DOS")==1);assert(appDirectoryNext(1,&entry)==1);
 reset();
 node("/DOS/Many",true);for(unsigned i=0;i<1025;++i)child("/DOS/Many",std::to_string(i)+".COM");
 handle=appDirectoryOpen("/DOS/Many");assert(handle>0);
 for(unsigned i=0;i<1024;++i)assert(appDirectoryNext(handle,&entry)==1);
 assert(appDirectoryNext(handle,&entry)==-1&&appDirectoryNext(handle,&entry)==-1);reset();
 node("/DOS/Exact",true);for(unsigned i=0;i<1024;++i)child("/DOS/Exact",std::to_string(i)+".COM");
 handle=appDirectoryOpen("/DOS/Exact");for(unsigned i=0;i<1024;++i)assert(appDirectoryNext(handle,&entry)==1);
 assert(appDirectoryNext(handle,&entry)==0);reset();
 node("/DOS/Bad",true);child("/DOS/Bad",std::string(256,'x'));
 handle=appDirectoryOpen("/DOS/Bad");assert(appDirectoryNext(handle,&entry)==-1&&appDirectoryNext(handle,&entry)==-1);reset();
 handle=appDirectoryOpen("/DOS");iterationError=true;assert(appDirectoryNext(handle,&entry)==-1);iterationError=false;
 entry.size=0;assert(appDirectoryNext(handle,&entry)==-1);entry.size=sizeof(entry);
 assert(appDirectoryNext(0,&entry)==-1&&appDirectoryNext(9,&entry)==-1);
 cancelled=true;assert(appDirectoryNext(handle,&entry)==-1&&appDirectoryOpen("/DOS")==-1);cancelled=false;
 onWorker=false;assert(appDirectoryNext(handle,&entry)==-1&&appDirectoryOpen("/DOS")==-1);onWorker=true;
 reset();assert(live==0);
 puts("Production ABI7 directory callbacks: scope, hidden/adjacent files, shared handles, cleanup, limits, cancellation and IO errors passed.");
}
