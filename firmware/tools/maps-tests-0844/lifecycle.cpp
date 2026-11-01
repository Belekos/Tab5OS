#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include "Tab5MapViewPolicy.h"
using String=std::string;using BaseType_t=int;using UBaseType_t=unsigned;using TaskHandle_t=void*;
struct Queue {int kind=0;};using StaticQueue_t=Queue;using QueueHandle_t=Queue*;
using StaticSemaphore_t=int;using SemaphoreHandle_t=int*;
constexpr int tskNO_AFFINITY=-1,pdTRUE=1,portMAX_DELAY=-1;namespace fs{struct File{};}
unsigned checks=0,delayMs=0,connections=0,allocated=0,deletions=0;bool foreground=false,requestTaken=false;unsigned retire=0;void* published=nullptr;
#define CHECK(c) do{++checks;if(!(c)){std::cerr<<__LINE__<<": "<<#c<<"\n";abort();}}while(0)
/* HEADER */
Tab5MapEngine* owner=nullptr;
uint32_t millis(){return 0;}
unsigned pdMS_TO_TICKS(unsigned n){return n;}
void vTaskDelay(unsigned n){delayMs+=n;}
void xSemaphoreTake(int*,int){}void xSemaphoreGive(int*){}
void tab5TaskCheckpoint(const char*){}
void vTaskDelete(void*){CHECK(!connections);++deletions;}
void heap_caps_free(void* p){if(p){CHECK(allocated);--allocated;free(p);}}
int uxQueueMessagesWaiting(Queue*){return 0;}
int xQueueSend(Queue*,Tab5MapTileResult* item,unsigned){CHECK(!published);published=item->data;return 1;}
template<class T>int xQueueSend(Queue*,T*,unsigned){return 1;}
template<class T>int xQueueReceive(Queue* queue,T*,unsigned){if(queue->kind==1&&!requestTaken){requestTaken=true;return 1;}if(queue->kind==1)owner->_active=false;return 0;}
bool tab5NetworkBackgroundYieldRequested(){return foreground;}bool tab5NetworkEnabled(){return true;}
constexpr int WL_CONNECTED=3;struct{int status(){return WL_CONNECTED;}}WiFi;
struct Tab5MapConnection{unsigned used=0;Tab5MapConnection(){++connections;}void close(){}~Tab5MapConnection(){CHECK(connections);--connections;}};
struct{template<class...T>void printf(const char*,T...){}}Tab5DebugSerial;
namespace tab5tile{constexpr unsigned MaxAttempts=3;template<class A,class B,class C,class D>bool downloadWithRetries(int&,uint8_t&,A call,B,C,D){return call();}}
void Tab5MapEngine::discardSearchRequests(){}
bool Tab5MapEngine::localSearch(const SearchRequest&,Tab5MapSearchResult&){CHECK(false);return false;}
bool Tab5MapEngine::downloadSearch(const SearchRequest&,Tab5MapSearchResult&,Tab5MapConnection&){CHECK(false);return false;}
bool Tab5MapEngine::downloadTile(const TileRequest&,Tab5MapTileResult& result,Tab5MapConnection&){result.data=static_cast<uint8_t*>(malloc(256));CHECK(result.data);++allocated;if(retire==1)owner->_active=false;if(retire==2)++owner->_generation;if(retire==3)foreground=true;return true;}
/* FUNCTIONS */
unsigned run(bool radio,unsigned cancel){
 Tab5MapEngine engine;owner=&engine;engine._active=true;engine._radioPriority=radio;engine._task=&engine;engine._lifecycleMutex=&engine._lifecycleStorage;
 Queue request{1},search{2},result{3},searchResult{4};engine._requestQueue=&request;engine._searchRequestQueue=&search;engine._resultQueue=&result;engine._searchResultQueue=&searchResult;
 delayMs=0;requestTaken=false;retire=cancel;foreground=false;published=nullptr;
 Tab5MapEngine::taskEntry(&engine);CHECK(!engine._task&&!connections);
 // A published result belongs to the UI; the cancellation branch must release it.
 if(!cancel||cancel==3){CHECK(allocated==1&&published);heap_caps_free(published);published=nullptr;}else CHECK(!published);
 CHECK(!allocated);
 return delayMs;
}
int main(){
 // Cancellation suppresses publication, so its result has an exact tracked owner.
 const unsigned cancelled=run(false,true),radioCancelled=run(true,true);
 const unsigned normal=run(false,false),radioNormal=run(true,false);
 CHECK(normal==60&&radioNormal==220);
 const unsigned stale=run(false,2),yielded=run(false,3);
 CHECK(!allocated&&deletions==6);
 std::cout<<"checks="<<checks<<" PASS cancelled_throttle_ms="<<cancelled<<" radio_cancelled_throttle_ms="<<radioCancelled<<" stale_generation_throttle_ms="<<stale<<" media_yield_throttle_ms="<<yielded<<"\n";
}
