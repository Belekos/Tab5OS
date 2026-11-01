#include "Tab5GeoIndex.h"
#include "Tab5MapViewPolicy.h"
#include "Tab5MapViewState.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <functional>
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
struct Queue{size_t capacity=0;bool created=false;uint8_t* supplied=nullptr;size_t itemBytes=0;std::mutex mutex;std::condition_variable changed;std::deque<std::vector<uint8_t>> entries;};
using StaticQueue_t=Queue;using QueueHandle_t=Queue*;using SemaphoreHandle_t=std::mutex*;using StaticSemaphore_t=std::mutex;using TaskHandle_t=void*;using BaseType_t=int;
constexpr int pdTRUE=1,portMAX_DELAY=-1,tskNO_AFFINITY=-1;
int failQueueCreate=-1;unsigned queueCreates=0,queueDeletes=0;
Queue* xQueueCreateStatic(unsigned n,unsigned bytes,uint8_t* storage,StaticQueue_t* control){++queueCreates;int f=failQueueCreate;if(f==0)return nullptr;if(f>0)--failQueueCreate;CHECK(n&&bytes&&storage&&control&&!control->created);control->capacity=n;control->created=true;control->supplied=storage;control->itemBytes=bytes;return control;}
void vQueueDelete(Queue* q){CHECK(q&&q->created);CHECK(q->entries.empty());q->created=false;++queueDeletes;}

unsigned uxQueueMessagesWaiting(Queue* q){std::lock_guard<std::mutex> lock(q->mutex);return q->entries.size();}
uint32_t millis(){static auto start=std::chrono::steady_clock::now();return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();}
unsigned pdMS_TO_TICKS(unsigned n){return n;}
void vTaskDelay(unsigned){std::this_thread::yield();}void vTaskDelete(void*){}
void tab5TaskCheckpoint(const char*){}
void xSemaphoreTake(std::mutex* mutex,int){mutex->lock();}void xSemaphoreGive(std::mutex* mutex){mutex->unlock();}
template<class T>int xQueueSend(Queue* q,const T* item,unsigned){std::lock_guard<std::mutex> lock(q->mutex);if(q->entries.size()>=q->capacity)return 0;const auto* p=reinterpret_cast<const uint8_t*>(item);q->entries.emplace_back(p,p+sizeof(T));q->changed.notify_all();return 1;}
void xQueueReset(Queue*q){std::lock_guard<std::mutex> lock(q->mutex);q->entries.clear();}
template<class T>int xQueueReceive(Queue* q,T* item,unsigned timeout){std::unique_lock<std::mutex> lock(q->mutex);if(q->entries.empty()&&timeout)q->changed.wait_for(lock,std::chrono::milliseconds(1));if(q->entries.empty())return 0;CHECK(q->entries.front().size()==sizeof(T));memcpy(item,q->entries.front().data(),sizeof(T));q->entries.pop_front();return 1;}
std::atomic<bool> storageAllowed{true},sdAvailable{true},fileMissing{false},isDirectory{false},networkYield{false},blockRead{false},readEntered{false};std::atomic<int> ioFail{-1};std::atomic<unsigned> storageUsers{0},filesOpen{0},httpCalls{0};std::atomic<size_t> ioBytes{0},ioMaxRead{0};std::string databasePath;
struct Tab5StorageUse{bool held;explicit Tab5StorageUse(bool){held=storageAllowed;if(held)++storageUsers;}~Tab5StorageUse(){if(held)--storageUsers;}explicit operator bool()const{return held;}};
bool tab5SdAvailable(){return sdAvailable;}
class File{struct Data{std::ifstream stream;size_t bytes;explicit Data(const std::string& path):stream(path,std::ios::binary){stream.seekg(0,std::ios::end);bytes=stream.tellg();stream.seekg(0);++filesOpen;}~Data(){--filesOpen;}};std::shared_ptr<Data> p;public:File(){}explicit File(const std::string& path){if(!fileMissing)p=std::make_shared<Data>(path);}explicit operator bool()const{return p&&bool(p->stream);}bool isDirectory()const{return ::isDirectory;}size_t size()const{return p?p->bytes:0;}bool seek(uint32_t at){if(!p||at>p->bytes)return false;p->stream.clear();p->stream.seekg(at);return bool(p->stream);}size_t read(void* dest,size_t count){readEntered=true;while(blockRead)std::this_thread::yield();int f=ioFail.load();if(f==0)return 0;if(f>0)--ioFail;CHECK(count<=4096);ioBytes+=count;ioMaxRead=std::max(ioMaxRead.load(),count);if(!p)return 0;p->stream.read(static_cast<char*>(dest),count);return p->stream.gcount();}void close(){p.reset();}};
namespace fs{using File=::File;}constexpr char FILE_READ[]="r";struct{File open(const char*,const char*){return File(databasePath);}}tab5FS;
namespace Tab5Meshtastic{struct Options{bool showOnMap=false;};struct Position{uint32_t node=0,timestamp=0;double latitude=0,longitude=0;char name[40]{};};Options opts;std::vector<Position> positions;uint32_t revision=0;Options options(){return opts;}size_t positionCount(){return positions.size();}bool position(size_t i,Position& p){if(i>=positions.size())return false;p=positions[i];return true;}uint32_t positionRevision(){return revision;}}
std::atomic<bool> networkEnabled{true},wifiConnected{true},loseWifiInHttp{false},blockHttp{false},httpEntered{false},blockTile{false},tileEntered{false};
std::atomic<int> httpMode{0};std::atomic<unsigned> tileCalls{0};
constexpr int WL_CONNECTED=3;struct{int status(){return wifiConnected?WL_CONNECTED:0;}}WiFi;
bool tab5NetworkBackgroundYieldRequested(){return networkYield;}bool tab5NetworkEnabled(){return networkEnabled;}
struct Tab5MapConnection{uint32_t used=0;void close(){}};
struct{template<class...T>void printf(const char*,T...){}}Tab5DebugSerial;
namespace tab5tile{constexpr unsigned MaxAttempts=3;template<class A,class B,class C,class D>bool downloadWithRetries(int&,uint8_t&,A call,B,C,D){return call();}}
/* HEADER */
bool Tab5MapEngine::ensureTask(){return _active.load();}
uint8_t Tab5MapEngine::styleMaxZoom(size_t)const{return 18;}
int8_t Tab5MapEngine::styleZoomShift(size_t)const{return 0;}
bool Tab5MapEngine::styleImportAllowed(size_t)const{return true;}
bool Tab5MapEngine::downloadTile(const TileRequest& request,Tab5MapTileResult& out,Tab5MapConnection&){
 ++tileCalls;tileEntered=true;while(blockTile)std::this_thread::yield();
 out.data=static_cast<uint8_t*>(heap_caps_malloc(256,3));CHECK(out.data);memset(out.data,int(request.generation&255),256);out.size=256;out.success=true;return true;
}
bool Tab5MapEngine::downloadSearch(const SearchRequest&,Tab5MapSearchResult& out,Tab5MapConnection&){
 CHECK(storageUsers==0&&filesOpen==0);++httpCalls;httpEntered=true;while(blockHttp)std::this_thread::yield();
 if(httpMode==3){wifiConnected=false;out.httpCode=-100;return false;}
 if(httpMode==1){out.items=static_cast<Tab5MapSearchItem*>(heap_caps_calloc(1,sizeof(Tab5MapSearchItem),3));CHECK(out.items);out.httpCode=503;out.success=false;return false;}
 if(httpMode==2){out.success=true;out.httpCode=200;out.count=0;return true;}
 out.items=static_cast<Tab5MapSearchItem*>(heap_caps_calloc(1,sizeof(Tab5MapSearchItem),3));CHECK(out.items);strcpy(out.items[0].name,"Online place");out.count=1;out.success=true;out.httpCode=200;return true;
}
/* FUNCTIONS */
Tab5MapSearchResult waitResult(Tab5MapEngine& e,uint32_t id){Tab5MapSearchResult r;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(4);do{if(e.pollSearch(r)){CHECK(r.requestId==id);return r;}std::this_thread::yield();}while(std::chrono::steady_clock::now()<end);CHECK(false);return r;}
void testQueues(){
 const size_t initial=allocated;
 {Tab5MapEngine e;failAllocation=0;CHECK(!e.createQueues());CHECK(!e._queueStorage&&!e._requestQueue);failAllocation=-1;CHECK(allocated==initial);}
 for(int fail=0;fail<4;++fail){Tab5MapEngine e;failQueueCreate=fail;CHECK(!e.createQueues());CHECK(!e._queueStorage&&!e._requestQueue&&!e._resultQueue&&!e._searchRequestQueue&&!e._searchResultQueue);CHECK(allocated==initial);}
 failQueueCreate=-1;Tab5MapEngine e;CHECK(e.createQueues());const size_t bytes=32*sizeof(Tab5MapEngine::TileRequest)+sizeof(Tab5MapTileResult)+2*sizeof(Tab5MapEngine::SearchRequest)+sizeof(Tab5MapSearchResult);CHECK(allocated-initial==bytes);CHECK(bytes<2048);
 unsigned calls=queueCreates;CHECK(e.createQueues()&&queueCreates==calls);Queue* queues[]={e._requestQueue,e._resultQueue,e._searchRequestQueue,e._searchResultQueue};size_t offset=0;for(auto*q:queues){CHECK(q->supplied==e._queueStorage+offset);offset+=q->capacity*q->itemBytes;vQueueDelete(q);}CHECK(offset==bytes);heap_caps_free(e._queueStorage);CHECK(allocated==initial);
}

bool waitFor(const std::function<bool()>& predicate){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);do{if(predicate())return true;std::this_thread::yield();}while(std::chrono::steady_clock::now()<end);return false;}
int main(int argc,char**argv){testQueues();CHECK(argc==2);databasePath=argv[1];Tab5MapEngine e;e._ready=true;e._active=true;e._lifecycleMutex=&e._lifecycleStorage;
 Queue tq{32},tr{1},sq{2},sr{1};e._requestQueue=&tq;e._resultQueue=&tr;e._searchRequestQueue=&sq;e._searchResultQueue=&sr;
 Tab5MapEngine::MapStyle style{};e._styles=&style;e._styleCount=1;
 e._places=static_cast<Tab5MapPlace*>(heap_caps_calloc(TAB5_MAP_MAX_PLACES,sizeof(Tab5MapPlace),3));CHECK(e._places);e._placeCount=1;strcpy(e._places[0].name,"Maison Lorient");e._places[0].latitude=47.75;e._places[0].longitude=-3.36;
 Tab5Meshtastic::Position mesh;mesh.node=0x12345678;mesh.timestamp=123456;mesh.latitude=47.7;mesh.longitude=-3.3;strcpy(mesh.name,"Radio Lorient");Tab5Meshtastic::positions.push_back(mesh);Tab5Meshtastic::opts.showOnMap=true;
 const size_t base=allocated;
 auto start=[&](){e._active=true;e._task=&e;return std::thread([&]{Tab5MapEngine::taskEntry(&e);});};
 auto stop=[&](std::thread& thread){e._active=false;e._searchRequestId=0;blockHttp=blockTile=blockRead=false;thread.join();e.discardSearchRequests();Tab5MapTileResult tile;while(e.poll(tile))e.release(tile);Tab5MapSearchResult stale;while(e.pollSearch(stale))e.releaseSearch(stale);CHECK(!e._task&&allocated==base&&filesOpen==0&&storageUsers==0);};
 // SCENARIOS
 heap_caps_free(e._places);e._places=nullptr;CHECK(allocated==0&&allocations.empty());
 std::cout<<"checks="<<checks<<" PASS; HTTP calls="<<httpCalls<<"; tile calls="<<tileCalls<<"; peak tracked PSRAM="<<peak<<"; read chunk max="<<ioMaxRead<<"; hardware not tested\n";
}
