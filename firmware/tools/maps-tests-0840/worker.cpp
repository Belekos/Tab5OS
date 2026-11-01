#include "Tab5GeoIndex.h"
#include "Tab5MapViewPolicy.h"
#include "Tab5MapViewState.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
std::atomic<size_t> checks{0};
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
class String:public std::string{public:using std::string::string;String(const std::string& s):std::string(s){}void trim(){auto a=find_first_not_of(" \t\r\n");if(a==npos){clear();return;}erase(find_last_not_of(" \t\r\n")+1);erase(0,a);}};
void copyUtf8(char* out,size_t n,const String& value){Tab5GeoIndex::copy(out,n,value.c_str());}
struct{String selectedId(){return "fr-FR";}}Tab5Locale;
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
std::atomic<int> failAllocation{-1};std::mutex allocationsMutex;std::map<void*,size_t> allocations;size_t allocated=0,peak=0;
void* heap_caps_malloc(size_t n,int caps){CHECK(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));int f=failAllocation.load();if(f==0)return nullptr;if(f>0)--failAllocation;void* p=malloc(n);if(p){std::lock_guard<std::mutex> lock(allocationsMutex);allocations[p]=n;allocated+=n;peak=std::max(peak,allocated);}return p;}
void* heap_caps_calloc(size_t count,size_t n,int caps){if(count&&n>SIZE_MAX/count)return nullptr;auto* p=heap_caps_malloc(count*n,caps);if(p)memset(p,0,count*n);return p;}
void heap_caps_free(void* p){if(!p)return;std::lock_guard<std::mutex> lock(allocationsMutex);auto it=allocations.find(p);CHECK(it!=allocations.end());allocated-=it->second;allocations.erase(it);free(p);}
struct Queue{size_t capacity;std::mutex mutex;std::condition_variable changed;std::deque<std::vector<uint8_t>> entries;};
using QueueHandle_t=Queue*;using SemaphoreHandle_t=std::mutex*;using StaticSemaphore_t=std::mutex;using TaskHandle_t=void*;using BaseType_t=int;
constexpr int pdTRUE=1,portMAX_DELAY=-1,tskNO_AFFINITY=-1;
unsigned uxQueueMessagesWaiting(Queue* q){std::lock_guard<std::mutex> lock(q->mutex);return q->entries.size();}
uint32_t millis(){static auto start=std::chrono::steady_clock::now();return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();}
unsigned pdMS_TO_TICKS(unsigned n){return n;}
void vTaskDelay(unsigned){std::this_thread::yield();}void vTaskDelete(void*){}
void xSemaphoreTake(std::mutex* mutex,int){mutex->lock();}void xSemaphoreGive(std::mutex* mutex){mutex->unlock();}
template<class T>int xQueueSend(Queue* q,const T* item,unsigned){std::lock_guard<std::mutex> lock(q->mutex);if(q->entries.size()>=q->capacity)return 0;const auto* p=reinterpret_cast<const uint8_t*>(item);q->entries.emplace_back(p,p+sizeof(T));q->changed.notify_all();return 1;}
template<class T>int xQueueReceive(Queue* q,T* item,unsigned timeout){std::unique_lock<std::mutex> lock(q->mutex);if(q->entries.empty()&&timeout)q->changed.wait_for(lock,std::chrono::milliseconds(1));if(q->entries.empty())return 0;CHECK(q->entries.front().size()==sizeof(T));memcpy(item,q->entries.front().data(),sizeof(T));q->entries.pop_front();return 1;}
std::atomic<bool> storageAllowed{true},sdAvailable{true},fileMissing{false},isDirectory{false},networkYield{false},blockRead{false},readEntered{false};std::atomic<int> ioFail{-1};std::atomic<unsigned> storageUsers{0},filesOpen{0},httpCalls{0};std::atomic<size_t> ioBytes{0},ioMaxRead{0};std::string databasePath;
struct Tab5StorageUse{bool held;explicit Tab5StorageUse(bool){held=storageAllowed;if(held)++storageUsers;}~Tab5StorageUse(){if(held)--storageUsers;}explicit operator bool()const{return held;}};
bool tab5SdAvailable(){return sdAvailable;}
class File{struct Data{std::ifstream stream;size_t bytes;explicit Data(const std::string& path):stream(path,std::ios::binary){stream.seekg(0,std::ios::end);bytes=stream.tellg();stream.seekg(0);++filesOpen;}~Data(){--filesOpen;}};std::shared_ptr<Data> p;public:File(){}explicit File(const std::string& path){if(!fileMissing)p=std::make_shared<Data>(path);}explicit operator bool()const{return p&&bool(p->stream);}bool isDirectory()const{return ::isDirectory;}size_t size()const{return p?p->bytes:0;}bool seek(uint32_t at){if(!p||at>p->bytes)return false;p->stream.clear();p->stream.seekg(at);return bool(p->stream);}size_t read(void* dest,size_t count){readEntered=true;while(blockRead)std::this_thread::yield();int f=ioFail.load();if(f==0)return 0;if(f>0)--ioFail;CHECK(count<=4096);ioBytes+=count;ioMaxRead=std::max(ioMaxRead.load(),count);if(!p)return 0;p->stream.read(static_cast<char*>(dest),count);return p->stream.gcount();}void close(){p.reset();}};
namespace fs{using File=::File;}constexpr char FILE_READ[]="r";struct{File open(const char*,const char*){return File(databasePath);}}tab5FS;
namespace Tab5Meshtastic{struct Options{bool showOnMap=false;};struct Position{uint32_t node=0,timestamp=0;double latitude=0,longitude=0;char name[40]{};};Options opts;std::vector<Position> positions;uint32_t revision=0;Options options(){return opts;}size_t positionCount(){return positions.size();}bool position(size_t i,Position& p){if(i>=positions.size())return false;p=positions[i];return true;}uint32_t positionRevision(){return revision;}}
bool tab5NetworkBackgroundYieldRequested(){return networkYield;}bool tab5NetworkEnabled(){return true;}
struct Tab5MapConnection{uint32_t used=0;void close(){}};
struct{template<class...T>void printf(const char*,T...){}}Tab5DebugSerial;
namespace tab5tile{constexpr unsigned MaxAttempts=3;template<class A,class B,class C,class D>bool downloadWithRetries(int&,uint8_t&,A call,B,C,D){return call();}}
/* HEADER */
bool Tab5MapEngine::ensureTask(){return _active.load();}
bool Tab5MapEngine::downloadTile(const TileRequest&,Tab5MapTileResult&,Tab5MapConnection&){return false;}
bool Tab5MapEngine::downloadSearch(const SearchRequest&,Tab5MapSearchResult& out,Tab5MapConnection&){CHECK(storageUsers==0&&filesOpen==0);++httpCalls;out.success=true;out.httpCode=200;return true;}
/* FUNCTIONS */
Tab5MapSearchResult waitResult(Tab5MapEngine& e,uint32_t id){Tab5MapSearchResult r;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(4);do{if(e.pollSearch(r)){CHECK(r.requestId==id);return r;}std::this_thread::yield();}while(std::chrono::steady_clock::now()<end);CHECK(false);return r;}
int main(int argc,char**argv){CHECK(argc==2);databasePath=argv[1];Tab5MapEngine e;e._ready=true;e._active=true;e._lifecycleMutex=&e._lifecycleStorage;
 Queue tq{8},tr{2},sq{2},sr{1};e._requestQueue=&tq;e._resultQueue=&tr;e._searchRequestQueue=&sq;e._searchResultQueue=&sr;
 e._places=static_cast<Tab5MapPlace*>(heap_caps_calloc(TAB5_MAP_MAX_PLACES,sizeof(Tab5MapPlace),3));CHECK(e._places);e._placeCount=2;strcpy(e._places[0].name,"Maison Lorient");e._places[0].latitude=47.75;e._places[0].longitude=-3.36;strcpy(e._places[1].name,"Paris home");e._places[1].latitude=48.8;e._places[1].longitude=2.3;
 Tab5Meshtastic::Position p;p.node=0x12345678;p.timestamp=123456;p.latitude=47.7;p.longitude=-3.3;strcpy(p.name,"Radio Lorient");Tab5Meshtastic::positions.push_back(p);
 CHECK(e.markerCount()==2);Tab5MapMarker m;CHECK(e.marker(0,m)&&m.kind==Tab5MapMarkerKind::Saved);auto rev=e.markerRevision();CHECK(e.markerRevision()==rev);
 Tab5Meshtastic::opts.showOnMap=true;CHECK(e.markerCount()==3);CHECK(e.markerRevision()!=rev);rev=e.markerRevision();CHECK(e.marker(2,m)&&m.node==p.node&&m.timestamp==p.timestamp&&m.kind==Tab5MapMarkerKind::Mesh);
 Tab5MapSearchItem chosen;strcpy(chosen.name,p.name);chosen.latitude=p.latitude;chosen.longitude=p.longitude;chosen.node=p.node;e.selectSearchMarker(chosen);CHECK(e.markerCount()==4);CHECK(e.markerRevision()!=rev);CHECK(e.marker(2,m)&&m.kind==Tab5MapMarkerKind::Search);
 Tab5Meshtastic::opts.showOnMap=false;CHECK(e.markerCount()==2);CHECK(!e.marker(2,m));Tab5Meshtastic::opts.showOnMap=true;
 // Maximum marker snapshot stays bounded and uses PSRAM.
 e._placeCount=64;for(unsigned i=2;i<64;++i){snprintf(e._places[i].name,sizeof(e._places[i].name),"Waypoint %u",i);e._places[i].latitude=47;e._places[i].longitude=-3;}
 Tab5Meshtastic::positions.resize(32,p);CHECK(e.markerCount()==97);size_t beforeSnapshot=allocated;CHECK(e.requestSearch("Lorient",7,47.7,-3.3));Tab5MapEngine::SearchRequest maximum{};CHECK(xQueueReceive(&sq,&maximum,0));CHECK(maximum.pointCount==97&&allocated-beforeSnapshot==97*sizeof(Tab5MapMarker));heap_caps_free(maximum.points);CHECK(allocated==beforeSnapshot);e._placeCount=2;Tab5Meshtastic::positions.resize(1);
 // Replacing queued snapshots frees exactly the previous owned buffer.
 const size_t base=allocated;CHECK(e.requestSearch("Lorient",1,47.7,-3.3));size_t one=allocated;CHECK(one>base);CHECK(e.requestSearch("Paris",2,48.8,2.3));CHECK(allocated==one);e.discardSearchRequests();CHECK(allocated==base);
 failAllocation=0;CHECK(!e.requestSearch("Paris",3,48.8,2.3));CHECK(allocated==base);failAllocation=-1;
 for(unsigned session=0;session<15;++session){e._active=true;e._task=&e;std::thread t([&]{Tab5MapEngine::taskEntry(&e);});
  for(unsigned i=0;i<5;++i){uint32_t id=100+session*10+i;CHECK(e.requestSearch(i%2?"Paris":"Lorient",id,47.7,-3.3));auto r=waitResult(e,id);CHECK(r.success&&r.local&&r.gazetteerAvailable&&!r.localError);CHECK(r.count&&r.items);if(i%2==0){bool mesh=false,saved=false;for(unsigned j=0;j<r.count;++j){mesh|=r.items[j].kind==Tab5MapMarkerKind::Mesh;saved|=r.items[j].kind==Tab5MapMarkerKind::Saved;}CHECK(mesh&&saved);}e.releaseSearch(r);CHECK(!r.items&&!r.count);}
  e._active=false;e._searchRequestId=0;t.join();CHECK(!e._task);CHECK(allocated==base&&filesOpen==0&&storageUsers==0);
 }
 CHECK(httpCalls==0);e._active=true;e._task=&e;std::thread t([&]{Tab5MapEngine::taskEntry(&e);});
 // Missing SD/index keeps saved and Mesh matches available, never calls HTTP.
 fileMissing=true;CHECK(e.requestSearch("Lorient",401,47.7,-3.3));auto r=waitResult(e,401);CHECK(r.success&&r.localError&&!r.gazetteerAvailable&&r.count>=2);e.releaseSearch(r);fileMissing=false;
 storageAllowed=false;CHECK(e.requestSearch("Lorient",402,47.7,-3.3));r=waitResult(e,402);CHECK(r.success&&r.localError&&r.count>=2);e.releaseSearch(r);storageAllowed=true;
 ioFail=0;CHECK(e.requestSearch("Lorient",403,47.7,-3.3));r=waitResult(e,403);CHECK(r.success&&r.localError);e.releaseSearch(r);ioFail=-1;
 networkYield=true;Tab5MapEngine::TileRequest waitingTile{};waitingTile.generation=e._generation;CHECK(xQueueSend(&tq,&waitingTile,0));std::this_thread::sleep_for(std::chrono::milliseconds(10));CHECK(e.requestSearch("Paris",404,48.8,2.3));r=waitResult(e,404);CHECK(r.success&&r.local&&r.gazetteerAvailable);e.releaseSearch(r);++e._generation;networkYield=false;
 size_t startBytes=ioBytes;CHECK(e.requestSearch("sa",408,0,0));r=waitResult(e,408);CHECK(r.success&&r.count==8);e.releaseSearch(r);CHECK(ioBytes-startBytes<3*1024*1024);
 CHECK(httpCalls==0);CHECK(e.requestSearch("Paris",405,48.8,2.3,true));r=waitResult(e,405);CHECK(r.success&&!r.local&&httpCalls==1);e.releaseSearch(r);
 networkYield=true;CHECK(e.requestSearch("Paris",411,48.8,2.3,true));std::this_thread::sleep_for(std::chrono::milliseconds(10));CHECK(e.requestSearch("Lorient",412,47.7,-3.3));r=waitResult(e,412);CHECK(r.success&&r.local&&httpCalls==1);e.releaseSearch(r);networkYield=false;
 // A query superseded during actual SD reading releases file/cache/snapshot.
 blockRead=true;readEntered=false;CHECK(e.requestSearch("sa",406,0,0));auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!readEntered&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();CHECK(readEntered);CHECK(e.requestSearch("Lorient",407,47.7,-3.3));blockRead=false;r=waitResult(e,407);CHECK(r.success&&r.local);e.releaseSearch(r);
 e._active=false;e._searchRequestId=0;t.join();CHECK(allocated==base&&filesOpen==0&&storageUsers==0);CHECK(ioMaxRead<=4096);
 // Direct actual local worker: reject result allocation; reject only cache allocation.
 e._active=true;e._searchRequestId=501;Tab5MapEngine::SearchRequest q{};q.requestId=501;strcpy(q.query,"Paris");q.latitude=48.8;q.longitude=2.3;Tab5MapSearchResult direct;
 failAllocation=0;CHECK(!e.localSearch(q,direct)&&!direct.items&&direct.httpCode==-103);failAllocation=1;direct={};CHECK(e.localSearch(q,direct)&&direct.localError&&!direct.gazetteerAvailable);e.releaseSearch(direct);failAllocation=-1;
 heap_caps_free(e._places);e._places=nullptr;CHECK(allocated==0&&allocations.empty());
 std::cout<<"checks="<<checks<<" PASS; peak tracked PSRAM="<<peak<<"; read chunk max="<<ioMaxRead<<"; physical hardware not tested\n";
}
