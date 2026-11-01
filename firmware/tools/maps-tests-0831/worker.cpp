// Actual engine request/result/task-loop functions are inserted by run.py.
// FreeRTOS queues/clock, network transfer and capability heap are host adapters.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
static std::atomic<unsigned> checks{0};
static void check(bool ok,const char* s){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",s);exit(1);}}
class String:public std::string {public:using std::string::string;String(const std::string& s):std::string(s){}void trim(){const auto first=find_first_not_of(" \t\r\n");if(first==npos){clear();return;}erase(find_last_not_of(" \t\r\n")+1);erase(0,first);}};
static void copyUtf8(char* p,size_t n,const String& s){snprintf(p,n,"%s",s.c_str());}
static struct{String selectedId(){return "fr-FR";}}Tab5Locale;
struct Queue{size_t capacity;std::mutex mutex;std::condition_variable changed;std::deque<std::vector<unsigned char>> entries;};
using QueueHandle_t=Queue*;using SemaphoreHandle_t=std::mutex*;using TaskHandle_t=void*;
static constexpr int pdTRUE=1,pdFALSE=0,portMAX_DELAY=-1;
static constexpr unsigned TAB5_MAP_MAX_SEARCH_RESULTS=8;
static std::atomic<uint32_t> ticks{1};static uint32_t millis(){return ticks.fetch_add(1);}
static unsigned pdMS_TO_TICKS(unsigned n){return n;}
static void vTaskDelay(unsigned n){ticks.fetch_add(n);std::this_thread::yield();}
static void xSemaphoreTake(SemaphoreHandle_t m,int){m->lock();}static void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
template<class T>static int xQueueSend(QueueHandle_t q,const T* item,unsigned){std::lock_guard<std::mutex> guard(q->mutex);if(q->entries.size()>=q->capacity)return pdFALSE;const auto* p=reinterpret_cast<const unsigned char*>(item);q->entries.emplace_back(p,p+sizeof(T));q->changed.notify_all();return pdTRUE;}
template<class T>static int xQueueReceive(QueueHandle_t q,T* item,unsigned timeout){std::unique_lock<std::mutex> guard(q->mutex);if(q->entries.empty()&&timeout)q->changed.wait_for(guard,std::chrono::milliseconds(1));if(q->entries.empty())return pdFALSE;check(q->entries.front().size()==sizeof(T),"queue payload has production size");memcpy(item,q->entries.front().data(),sizeof(T));q->entries.pop_front();return pdTRUE;}
static void xQueueReset(QueueHandle_t q){std::lock_guard<std::mutex> guard(q->mutex);q->entries.clear();}
static void vTaskDelete(void*){}
static std::atomic<bool> cancelEntered{false};
static std::atomic<unsigned> aliveResults{0},peakResults{0},completed{0},networkContexts{0};
static void heap_caps_free(void* p){if(p){check(aliveResults.fetch_sub(1)>0,"owned result released once");free(p);}}
static bool tab5NetworkBackgroundYieldRequested(){return false;}static bool tab5NetworkEnabled(){return true;}static void tab5TaskCheckpoint(const char*){}
struct Tab5MapConnection{uint32_t used=0;Tab5MapConnection(){++networkContexts;}void close(){}~Tab5MapConnection(){--networkContexts;}};
struct{template<class...T>void printf(const char*,T...){}}Tab5DebugSerial;
namespace tab5tile{constexpr unsigned MaxAttempts=3;template<class A,class B,class C,class D>bool downloadWithRetries(int&,uint8_t&,A download,B,C,D){return download();}}
/* ITEM */
/* SEARCH_RESULT */
/* TILE_RESULT */
class Tab5MapEngine {
public:
/* SEARCH_REQUEST */
/* TILE_REQUEST */
 std::atomic<bool> _ready{true},_active{true};std::atomic<uint32_t>_generation{1},_searchRequestId{0};std::atomic<bool>_radioPriority{false};
 Queue requests{8},results{2},searchRequests{2},searchResults{1};QueueHandle_t _requestQueue=&requests,_resultQueue=&results,_searchRequestQueue=&searchRequests,_searchResultQueue=&searchResults;
 std::mutex lifecycle;SemaphoreHandle_t _lifecycleMutex=&lifecycle;TaskHandle_t _task=nullptr;
 bool ensureTask(){return _active.load();}
 bool requestSearch(const String&,uint32_t);bool pollSearch(Tab5MapSearchResult&);void releaseSearch(Tab5MapSearchResult&);void taskLoop();static void taskEntry(void*);
 void release(Tab5MapTileResult& r){if(r.data)heap_caps_free(r.data);r.data=nullptr;}
 bool downloadTile(const TileRequest&,Tab5MapTileResult&,Tab5MapConnection&){return false;}
 bool downloadSearch(const SearchRequest& request,Tab5MapSearchResult& result,Tab5MapConnection&){
  check(!strcmp(request.language,"fr-FR"),"language snapshotted at submission");
  if(!strcmp(request.query,"cancel")){cancelEntered=true;while(_active&&_searchRequestId==request.requestId)std::this_thread::yield();result.httpCode=-107;return false;}
  if(!strcmp(request.query,"failure")){++completed;result.httpCode=-103;return false;}
  auto* items=static_cast<Tab5MapSearchItem*>(calloc(TAB5_MAP_MAX_SEARCH_RESULTS,sizeof(Tab5MapSearchItem)));check(items,"host result allocated");
  const unsigned alive=++aliveResults;unsigned peak=peakResults;while(peak<alive&&!peakResults.compare_exchange_weak(peak,alive)){}
  result.items=items;result.count=1;result.httpCode=200;snprintf(items[0].name,sizeof(items[0].name),"%s",request.query);items[0].latitude=request.requestId;result.success=true;++completed;return true;
 }
};
/* FUNCTIONS */
static Tab5MapSearchResult waitResult(Tab5MapEngine& engine,uint32_t id){
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);Tab5MapSearchResult r{};
 do{if(engine.pollSearch(r)){check(r.requestId==id,"result belongs to requested query");return r;}std::this_thread::yield();}while(std::chrono::steady_clock::now()<deadline);
 check(false,"result delivered before host timeout");return r;
}
int main(){
 Tab5MapEngine engine;
 for(unsigned session=0;session<40;++session){
  engine._active=true;engine._task=&engine;std::thread worker([&]{Tab5MapEngine::taskEntry(&engine);});
  for(unsigned cycle=0;cycle<10;++cycle){
   uint32_t id=session*100+cycle+1;String query=cycle%3==0?"Saint Nazaire":cycle%3==1?"Le Havre":"failure";
   check(engine.requestSearch(query,id),"successive search admitted by actual request function");auto r=waitResult(engine,id);
   check(r.success==(query!="failure"),"network success and failure both return to result loop");
   if(r.success)check(r.items&&r.count==1&&!strcmp(r.items[0].name,query.c_str())&&r.items[0].latitude==id,"result payload survives queue transfer");
   engine.releaseSearch(r);check(!r.items&&!r.count,"release clears public ownership");
  }
  // Replace a request while its transfer is still in progress.
  const uint32_t cancelId=session*100+50,nextId=cancelId+1;
  cancelEntered=false;check(engine.requestSearch("cancel",cancelId),"pending cancellable search admitted");
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while(!cancelEntered&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
  check(cancelEntered,"first transfer really entered before cancellation");
  check(engine.requestSearch("New York",nextId),"new query supersedes pending query");
  auto r=waitResult(engine,nextId);check(r.success&&!strcmp(r.items[0].name,"New York"),"superseding query completes");engine.releaseSearch(r);
  {std::lock_guard<std::mutex> guard(engine.lifecycle);engine._active=false;engine._searchRequestId=0;}
  worker.join();check(!engine._task,"worker handle relinquished after full loop cleanup");
  check(networkContexts==0&&aliveResults==0,"session closes context and returns every result allocation");
 }
 check(!engine.requestSearch("Paris",0),"zero request ID rejected");engine._active=true;
 check(!engine.requestSearch("x",1),"short query rejected");check(!engine.requestSearch(String(385,'x'),2),"oversized query rejected");
 printf("PASS %u checks, 400 sequential searches, 40 cancellation/replacement cycles, 40 worker stop/reopen sessions, peak result buffers %u, actual taskLoop/taskEntry/request/poll/release, scripted network and host queues\n",checks.load(),peakResults.load());
}
