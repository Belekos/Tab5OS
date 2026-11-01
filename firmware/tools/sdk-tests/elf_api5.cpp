#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "Tab5AppApi.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(2);}}while(0)
static bool owningWorker=true,cooperating=true,closed=false;static unsigned controls=0,calls=0,clockMs=0;
std::atomic<bool> cancelled{false};
struct Context{bool extended=false,panel=false;unsigned abi=1;tab5_app_api3 api3{};tab5_app_api4 api4{};tab5_app_api5 api5{};} storage;
Context* context=&storage;
bool worker(){return owningWorker;}bool cooperate(){return owningWorker&&cooperating;}
uint32_t millis(){return clockMs;}void vTaskDelay(int){++clockMs;}
tm* gmtime_r(const time_t* value,tm* out){return gmtime_s(out,value)==0?out:nullptr;}
tm* localtime_r(const time_t* value,tm* out){return localtime_s(out,value)==0?out:nullptr;}
uint64_t appFloating(unsigned,uint64_t,uint64_t){return 42;}
namespace Tab5NativeView{void extendedControls(bool value){CHECK(value);++controls;}}
namespace Tab5AppPanel {
bool closing(){++calls;return closed;}
int open(const tab5_app_panel_spec*){++calls;return 10;}
int poll(tab5_app_panel_event*){++calls;return 11;}
int write(const char*,size_t,int){++calls;return 12;}
int state(uint32_t){++calls;return 13;}
int network(int argc,const char*const*argv,void(*output)(void*,const char*,size_t),void*user,bool(*stop)()){
 ++calls;CHECK(argc==1&&std::strcmp(argv[0],"dns")==0);if(stop())return 130;output(user,"ok",2);return 14;
}
size_t translate(const char*,char*out,size_t cap){++calls;if(cap>2)std::memcpy(out,"ok",3);return 2;}
}
#include "elf-panel-closing-production.inc"
#include "elf-stop-production.inc"
#include "elf-sleep-production.inc"
#include "elf-api4-prepare-production.inc"
#include "elf-api4-production.inc"
#include "elf-api5-production.inc"
int main(){
 storage.api3.base.base.language="fr";storage.api3.base.base.should_stop=appStop;
 // Legacy ABI callers do not lock the form broker for each input/frame poll.
 CHECK(appStop()==0&&calls==0);CHECK(appSleep(3)==0&&calls==0&&clockMs==3);
 owningWorker=false;CHECK(appApi5()==nullptr&&controls==0&&!storage.panel);owningWorker=true;
 auto* api=appApi5();CHECK(api&&storage.panel&&storage.extended&&storage.abi==5&&controls==0);
 CHECK(api->base.base.base.base.version==5&&api->base.base.base.base.size==sizeof(tab5_app_api5));
 CHECK(api->base.base.base.base.should_stop==appStop&&api->base.base.base.base.language==storage.api3.base.base.language);
 CHECK(api->base.floating(0,0,0)==42);int calendar[9]{};CHECK(api->base.calendar(0,1,calendar)==0&&calendar[5]==70);
 CHECK(api->panel_open(nullptr)==10);CHECK(api->panel_poll(nullptr)==11);CHECK(api->panel_write("x",1,1)==12);CHECK(api->panel_state(TAB5_PANEL_DONE)==13);
 char translated[3]{};CHECK(api->translate("key",translated,sizeof(translated))==2&&std::strcmp(translated,"ok")==0);
 const char* args[]={"dns"};unsigned emitted=0;auto emit=[](void* u,const char* bytes,size_t n){CHECK(std::strcmp(bytes,"ok")==0&&n==2);++*static_cast<unsigned*>(u);};
 CHECK(api->network(1,args,emit,&emitted)==14&&emitted==1);
 cancelled=true;CHECK(api->network(1,args,emit,&emitted)==130&&emitted==1);unsigned old=calls;CHECK(api->panel_open(nullptr)==-ECANCELED&&calls==old);CHECK(appStop()==1);cancelled=false;
 closed=true;CHECK(appStop()==1&&appSleep(200)==1);closed=false;
 auto* api4=appApi4();CHECK(api4&&controls==1&&storage.panel);closed=true;CHECK(appStop()==1);closed=false;
 cooperating=false;old=calls;CHECK(api->panel_open(nullptr)==-EPERM);CHECK(api->panel_poll(nullptr)==-EPERM);CHECK(api->panel_write("x",1,0)==-EPERM);CHECK(api->panel_state(0)==-EPERM);CHECK(api->network(1,args,emit,&emitted)==-EPERM);CHECK(api->translate("x",translated,3)==0);CHECK(calls==old);
 cooperating=true;owningWorker=false;CHECK(appStop()==1&&appSleep(200)==1);
 std::printf("{\"passed\":true,\"checks\":%u,\"abiPrefixPreserved\":true,\"workerOnly\":true,\"synchronousNetworkCallback\":true,\"legacyNoPanelLock\":true,\"noGameOverlayForApi5\":true}\n",checks);
}
