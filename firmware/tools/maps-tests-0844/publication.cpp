#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
unsigned checks=0;
#define CHECK(c) do{++checks;if(!(c)){std::cerr<<__LINE__<<": "<<#c<<" errno="<<errno<<"\n";abort();}}while(0)
class String:public std::string {public:using std::string::string;String(const std::string& s):std::string(s){}String(unsigned n):std::string(std::to_string(n)){}String(uint8_t n):std::string(std::to_string(unsigned(n))){};};
struct Node{bool directory=false;std::string bytes;bool writeError=false;};
struct Faults{int write=0,flush=0,close=0,mkdir=0;bool overflowWrite=false,overflowRead=false;std::string stat,rename,rollback;};Faults fault;
std::map<std::string,std::shared_ptr<Node>> nodes;unsigned handles=0,faults=0;int lastError=0;
class File{std::shared_ptr<Node> node;public:File(){}explicit File(std::shared_ptr<Node> n):node(n){if(node)++handles;}File(const File&)=delete;File& operator=(const File&)=delete;File(File&& f):node(std::move(f.node)){}~File(){close();}explicit operator bool()const{return bool(node);}bool isDirectory()const{return node&&node->directory;}size_t size(){return node?node->bytes.size():0;}size_t read(uint8_t*,size_t count){if(fault.overflowRead){errno=EBADF;return size_t(-1);}return 0;}size_t write(const uint8_t* bytes,size_t count){CHECK(node);if(fault.overflowWrite)return count+1;if(fault.write){errno=fault.write;node->writeError=true;return 0;}node->bytes.append(reinterpret_cast<const char*>(bytes),count);return count;}bool getWriteError(){return node&&node->writeError;}void flush(){errno=fault.flush;}void close(){if(node){CHECK(handles);--handles;node.reset();errno=fault.close;}}};
const char* FILE_READ="r";const char* FILE_WRITE="w";
struct Fs{
 bool exists(const char* p){if(fault.stat==p){errno=EIO;return false;}if(nodes.count(p))return true;errno=ENOENT;return false;}
 bool mkdir(const char* p){if(fault.mkdir){errno=fault.mkdir;return false;}auto it=nodes.find(p);if(it!=nodes.end()){errno=EEXIST;return false;}auto n=std::make_shared<Node>();n->directory=true;nodes[p]=n;return true;}
 File open(const char* p,const char* mode){if(mode==FILE_WRITE){auto n=std::make_shared<Node>();nodes[p]=n;return File(n);}auto it=nodes.find(p);if(it!=nodes.end())return File(it->second);errno=ENOENT;return {};}
 bool remove(const char* p){auto it=nodes.find(p);if(it==nodes.end()){errno=ENOENT;return false;}nodes.erase(it);return true;}
 bool rename(const char* a,const char* b){if(fault.rename==a||fault.rollback==a){errno=ETIMEDOUT;return false;}auto it=nodes.find(a);if(it==nodes.end()){errno=ENOENT;return false;}if(nodes.count(b)){errno=EEXIST;return false;}nodes[b]=it->second;nodes.erase(it);return true;}
}tab5FS;
void tab5StorageFault(const char*,const char*,int error=0){++faults;lastError=error?error:(errno?errno:EIO);}
void delay(unsigned){}
constexpr size_t MAP_SD_IO_CHUNK=32768;
/* HELPERS */
constexpr const char* MAP_ROOT="/System/maps",*MAP_CACHE_ROOT="/System/maps/cache";
struct Tab5MapTileResult{uint8_t style=0,zoom=0;uint32_t x=0,y=0;bool success=true;const uint8_t* data=nullptr;size_t size=0;};
struct Tab5MapEngine{struct Style{char cacheId[24]="osm";};bool _ready=true,_sdAvailable=true;unsigned _styleCount=1;Style _styles[1];bool ensureCacheDirectories(uint8_t,uint8_t,uint32_t);String cachePath(uint8_t,uint8_t,uint32_t,uint32_t)const;bool storeTile(const Tab5MapTileResult&);void rememberTile(uint8_t,uint8_t,uint32_t,uint32_t,const uint8_t*,size_t){};};
bool validTile(const uint8_t* bytes,size_t count){return bytes&&count;}
/* FUNCTIONS */
std::string path="/System/maps/cache/osm/0/0/0.png";
void reset(){CHECK(!handles);nodes.clear();fault={};faults=0;lastError=0;errno=0;auto n=std::make_shared<Node>();n->bytes="previous";nodes[path]=n;auto b=std::make_shared<Node>();b->bytes="backup";nodes[path+".bak"]=b;}
void unchanged(){CHECK(nodes.count(path)&&nodes[path]->bytes=="previous");CHECK(!handles);}
int main(){
 Tab5MapEngine engine;const uint8_t bytes[]="replacement";Tab5MapTileResult tile;tile.data=bytes;tile.size=sizeof(bytes)-1;
 reset();CHECK(engine.storeTile(tile));CHECK(nodes[path]->bytes=="replacement"&&!nodes.count(path+".bak")&&!nodes.count(path+".part")&&!handles&&!faults);
 for(int i=0;i<100;++i){CHECK(engine.storeTile(tile));CHECK(nodes[path]->bytes=="replacement"&&!handles&&!faults);}
 for(int error:{ENOSPC,ENOMEM,EIO}){reset();fault.write=error;fault.flush=EIO;CHECK(!engine.storeTile(tile));unchanged();CHECK(lastError==error&&errno==error&&faults==1);CHECK(nodes[path+".bak"]->bytes=="backup");}
 for(int error:{ENOSPC,ENOMEM,EIO}){reset();fault.flush=error;CHECK(!engine.storeTile(tile));unchanged();CHECK(lastError==error&&errno==error&&faults==1);}
 reset();fault.close=ETIMEDOUT;CHECK(!engine.storeTile(tile));unchanged();CHECK(lastError==ETIMEDOUT&&faults==1);
 for(const auto& p:{path,path+".bak",path+".part"}){reset();fault.stat=p;CHECK(!engine.storeTile(tile));unchanged();CHECK(lastError==EIO&&faults==1);CHECK(nodes[path+".bak"]->bytes=="backup");}
 reset();fault.rename=path+".part";CHECK(!engine.storeTile(tile));unchanged();CHECK(lastError==ETIMEDOUT&&faults==1);
 reset();fault.rename=path+".part";fault.rollback=path+".bak";CHECK(!engine.storeTile(tile));CHECK(!nodes.count(path)&&nodes[path+".bak"]->bytes=="previous"&&!handles&&lastError==ETIMEDOUT);
 reset();fault.mkdir=ENOSPC;CHECK(!engine.storeTile(tile));unchanged();CHECK(lastError==ENOSPC&&faults==1);
 // EEXIST is accepted only for a directory; a regular file is never a cache directory.
 reset();auto regular=std::make_shared<Node>();nodes[MAP_ROOT]=regular;CHECK(!engine.storeTile(tile));unchanged();CHECK(lastError==EEXIST&&faults==1);
 reset();fault.overflowWrite=true;CHECK(!engine.storeTile(tile));unchanged();CHECK(lastError==EIO);
 reset();fault.overflowRead=true;File readFile=tab5FS.open(path.c_str(),FILE_READ);uint8_t destination[4];CHECK(!readFileCooperatively(readFile,destination,sizeof(destination))&&errno==EBADF);readFile.close();CHECK(!handles);
 std::cout<<"checks="<<checks<<" PASS; repeated_cache_writes=100; publication_faults_preserve_previous_and_errno\n";
}
