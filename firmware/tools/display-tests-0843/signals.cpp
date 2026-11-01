#include "Tab5DisplaySignals.h"
#include "esp_intr_alloc.h"
#include "hal/mipi_dsi_brg_ll.h"
#include "soc/mipi_dsi_periph.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
BridgeMock bridgeMock;
DsiSignals soc_mipi_dsi_signals[1]={{42}};
extern intr_handler_t sdkHandler;
extern void* sdkArgument;
extern int sdkSource,sdkFlags,sdkError,sdkCalls;
extern intr_handle_t* sdkResult;
static uint64_t clockUs=0;
static unsigned checks=0,originalCalls=0;
static void* argument=reinterpret_cast<void*>(77);
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"signals check line %d: %s\n",__LINE__,#x);abort();}}while(0)
int64_t esp_timer_get_time(){return static_cast<int64_t>(clockUs);}
static void original(void* arg){CHECK(arg==argument);++originalCalls;++bridgeMock.acknowledgements;bridgeMock.status=0;}
int main(int argc,char** argv){
 const bool wrapped=argc>1&&!strcmp(argv[1],"wrapped");
 CHECK(!tab5DisplaySignalsStats().active);
 intr_handle_t handle=nullptr;
 CHECK(esp_intr_alloc(8,123,original,argument,&handle)==ESP_OK);
 CHECK(sdkSource==8&&sdkFlags==123&&sdkHandler==original&&sdkArgument==argument&&sdkResult==&handle&&handle==reinterpret_cast<void*>(123));
 CHECK(!tab5DisplaySignalsStats().active);
 sdkError=9;CHECK(esp_intr_alloc(42,456,original,argument,&handle)==9);CHECK(!tab5DisplaySignalsStats().active);
 sdkError=0;CHECK(esp_intr_alloc(42,456,original,argument,&handle)==ESP_OK);
 CHECK(sdkSource==42&&sdkFlags==456&&sdkArgument==argument&&sdkResult==&handle);
 CHECK(tab5DisplaySignalsStats().active==wrapped);
 // The production observer sees the status before the original clears it;
 // only the original is allowed to acknowledge or modify the register.
 const uint32_t statuses[]={0,2,1,3,8,1};
 for(unsigned i=0;i<6;++i){clockUs=0x100000000ull+i*0x12345678ull;bridgeMock.status=statuses[i];sdkHandler(sdkArgument);CHECK(originalCalls==i+1&&bridgeMock.acknowledgements==i+1&&bridgeMock.status==0);}
 auto s=tab5DisplaySignalsStats();
 if(wrapped){CHECK(s.interrupts==6&&s.underruns==3&&s.lastStatus==1);CHECK(s.lastUnderrunUs==clockUs&&bridgeMock.reads==6);}
 else {CHECK(!s.interrupts&&!s.underruns&&!s.lastUnderrunUs&&!bridgeMock.reads);}
 // Reading a snapshot never reads/clears hardware or installs another ISR.
 int calls=sdkCalls;unsigned reads=bridgeMock.reads;
 for(unsigned i=0;i<1000;++i){auto copy=tab5DisplaySignalsStats();CHECK(copy.active==wrapped&&copy.underruns==s.underruns);}
 CHECK(calls==sdkCalls&&reads==bridgeMock.reads);
 printf("signals %s checks=%u original_calls=%u status_reads=%u acknowledgements=%u underruns=%u\n",wrapped?"wrapped":"unwrapped",checks,originalCalls,bridgeMock.reads,bridgeMock.acknowledgements,s.underruns);
}
