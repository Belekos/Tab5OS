#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
using std::min;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
class String:public std::string{public:using std::string::string;String(const std::string&s):std::string(s){}void trim(){auto at=find_first_not_of(" \t\r\n");if(at==npos){clear();return;}erase(find_last_not_of(" \t\r\n")+1);erase(0,at);}};
struct lv_obj_t{};struct lv_event_t{uintptr_t data=0;};void* lv_event_get_user_data(lv_event_t*e){return reinterpret_cast<void*>(e->data);}unsigned textLimit=0;void lv_textarea_set_max_length(lv_obj_t*,unsigned n){textLimit=n;}
/* TYPES */
constexpr unsigned TAB5_MAP_MAX_SEARCH_RESULTS=8;
enum class Page{Home,Map,Settings};enum class TextDialogAction{MapSearch};
Page activePage=Page::Map;lv_obj_t pageObject,modalObject,inputObject;lv_obj_t* mapPage=&pageObject,*currentScope=mapPage,*dialogInput=nullptr,*mapStatusLabel=nullptr;
lv_obj_t* keyboardScope(){return currentScope;}
bool mapSearchPending=false,mapSearchResultsReady=false,mapSearchLocal=false,mapGazetteerAvailable=false,mapSearchLocalError=false,mapSearchTruncated=false,mapOpenPending=false,sdReady=true,openAllowed=true;
uint32_t mapSearchRequestId=0,mapGeneration=1;uint8_t mapSearchCount=0,mapZoom=0;double mapLatitude=0,mapLongitude=0;String mapSearchQuery;
Tab5MapSearchItem rows[8];Tab5MapSearchItem* mapSearchItems=rows;
unsigned shown=0,toasts=0,released=0,requests=0,saved=0,freed=0,styles=0,renders=0,dialogs=0,closed=0,selected=0,dirty=0;
void toast(const char*,unsigned=0){++toasts;}const char*T(const char*s){return s;}void setLabelIfChanged(lv_obj_t*,const char*){}void updateMapLabels(){}void showMapSearchResults(){++shown;currentScope=&modalObject;}void closeMapSearchEvent(lv_event_t*){++closed;currentScope=mapPage;}void openTextDialog(TextDialogAction,const char*,const String&,const char*){++dialogs;currentScope=&modalObject;dialogInput=&inputObject;}void closeTextDialog(){currentScope=mapPage;dialogInput=nullptr;}
void saveMapLeavingView(){++saved;}void freeMapCanvas(){++freed;}void closeMapStylesEvent(lv_event_t*){currentScope=mapPage;}void clampMapView(){}void updateMapAttribution(){}void renderMapViewport(bool){++renders;}
void setKeyboardVisible(bool){}void showPage(Page p){if(openAllowed)activePage=p;}void markMapViewDirty(){++dirty;}
struct Engine{bool available=true,active=true,requestOk=true;std::deque<Tab5MapSearchResult> results;Tab5MapSearchItem selectedPoint;bool ready(){return available;}bool requestSearch(const String&,uint32_t,double,double,bool){++requests;return requestOk;}bool pollSearch(Tab5MapSearchResult& out){if(results.empty())return false;out=results.front();results.pop_front();return true;}void releaseSearch(Tab5MapSearchResult&r){++released;free(r.items);r.items=nullptr;r.count=0;}void setActive(bool yes,uint32_t){active=yes;if(!yes){while(!results.empty()){auto r=results.front();results.pop_front();releaseSearch(r);}}}size_t styleCount(){return 3;}void clearMemoryCache(){}void selectStyle(size_t){++styles;}void selectSearchMarker(const Tab5MapSearchItem&p){selectedPoint=p;++selected;}}Tab5Map;
struct{template<class...T>void printf(const char*,T...){}}Tab5DebugSerial;
namespace Tab5Meshtastic{struct Options{bool showOnMap=false;};Options value;Options options(){return value;}void setOptions(Options x){value=x;}}
/* FUNCTIONS */
void post(uint32_t id,bool success=true){Tab5MapSearchResult r;r.requestId=id;r.success=success;r.local=true;r.gazetteerAvailable=true;r.count=1;r.items=static_cast<Tab5MapSearchItem*>(calloc(1,sizeof(Tab5MapSearchItem)));strcpy(r.items[0].name,"Lorient");r.items[0].latitude=47.75;r.items[0].longitude=-3.36;Tab5Map.results.push_back(r);}
int main(){
 // A canceled request must neither leave a permanent spinner nor reopen results.
 mapSearchPending=mapSearchResultsReady=true;mapSearchRequestId=UINT32_MAX;mapOpenPending=true;leave(Page::Map,Page::Home);CHECK(!mapSearchPending&&!mapSearchResultsReady&&!mapOpenPending&&mapSearchRequestId==1&&!Tab5Map.active&&saved==1&&freed==1);
 mapSearchPending=mapSearchResultsReady=true;mapSearchRequestId=50;lv_event_t event;event.data=1;mapStyleSelectEvent(&event);CHECK(!mapSearchPending&&!mapSearchResultsReady&&mapSearchRequestId==51&&styles==1&&Tab5Map.active);
 // A new search input invalidates an old result before opening the editor.
 mapSearchPending=mapSearchResultsReady=true;mapSearchRequestId=77;post(77);mapSearchEvent(nullptr);CHECK(!mapSearchPending&&!mapSearchResultsReady&&mapSearchRequestId==78&&dialogs==1&&textLimit==96);poll();present();CHECK(shown==0&&released==1&&currentScope==&modalObject);
 // Current results arriving behind any modal remain pending for presentation.
 mapSearchRequestId=80;mapSearchPending=true;post(80);poll();CHECK(!mapSearchPending&&mapSearchResultsReady&&mapSearchCount==1&&!strcmp(rows[0].name,"Lorient"));present();CHECK(shown==0&&mapSearchResultsReady);currentScope=mapPage;present();CHECK(shown==1&&!mapSearchResultsReady);present();CHECK(shown==1);
 // Leaving before a deferred result is shown drops presentation permanently.
 mapSearchResultsReady=true;leave(Page::Map,Page::Settings);activePage=Page::Map;currentScope=mapPage;present();CHECK(shown==1&&!mapSearchResultsReady);
 // A delayed obsolete result is still released without replacing the current rows.
 mapSearchRequestId=90;mapSearchPending=true;post(89);unsigned oldReleased=released;poll();CHECK(mapSearchPending&&released==oldReleased+1&&!mapSearchResultsReady);
 post(90,false);poll();CHECK(!mapSearchPending&&!mapSearchResultsReady&&toasts==1);
 // Submit branch actually sends a local request with the current map center.
 currentScope=&modalObject;mapStatusLabel=&pageObject;submit("  Lorient  ");CHECK(requests==1&&mapSearchPending&&mapSearchQuery=="Lorient"&&currentScope==mapPage);Tab5Map.requestOk=false;submit("Paris");CHECK(!mapSearchPending&&toasts==2);submit("x");CHECK(requests==2&&toasts==3);
 // Explicit open-from-Mesh changes the option only after the map opens.
 activePage=Page::Home;sdReady=false;meshOpen(123,47,-3);CHECK(activePage==Page::Home&&!Tab5Meshtastic::value.showOnMap&&selected==0);
 sdReady=true;Tab5Map.available=false;meshOpen(123,47,-3);CHECK(!Tab5Meshtastic::value.showOnMap&&selected==0);Tab5Map.available=true;openAllowed=false;meshOpen(123,47,-3);CHECK(!Tab5Meshtastic::value.showOnMap&&selected==0);
 openAllowed=true;meshOpen(123,47,-3);CHECK(activePage==Page::Map&&Tab5Meshtastic::value.showOnMap&&mapLatitude==47&&mapLongitude==-3&&mapZoom==15&&mapOpenPending&&selected==1&&Tab5Map.selectedPoint.node==123&&Tab5Map.selectedPoint.kind==Tab5MapMarkerKind::Mesh);
 std::cout<<"checks="<<checks<<" PASS\n";
}
