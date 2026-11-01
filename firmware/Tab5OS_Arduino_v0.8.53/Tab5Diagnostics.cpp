#include "Tab5Diagnostics.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <esp_attr.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace {
constexpr uint32_t Magic=0x35444742; // BGD5, not a firmware version.
constexpr uint16_t Format=5;
constexpr unsigned Slots=8,VaultSlot=6,GitSlot=7,PhaseCapacity=32,DetailCapacity=80;
struct Stage {
 uint32_t sequence;
 uint8_t active;
 char phase[PhaseCapacity];
 char detail[DetailCapacity];
 uint8_t reserved[3];
};
struct Record {
 uint32_t magic;
 uint16_t version,size;
 uint32_t sequence;
 Stage stages[Slots];
 uint32_t checksum;
};
static_assert(sizeof(Record)<1024,"RTC breadcrumbs must stay below 1 KiB");
// Do not add an initializer: a C++ startup initialization would erase evidence.
RTC_NOINIT_ATTR Record retained;
portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
bool ready=false;
char previous[1024]{};

uint32_t checksum(const Record& value){
 // FNV-1a detects incomplete writes/corruption; this is not a security primitive.
 const auto* data=reinterpret_cast<const uint8_t*>(&value);
 uint32_t hash=2166136261U;
 for(size_t i=0;i<offsetof(Record,checksum);++i)hash=(hash^data[i])*16777619U;
 return hash;
}
bool valid(const Record& value){
 if(value.magic!=Magic||value.version!=Format||value.size!=sizeof(Record)||value.checksum!=checksum(value))return false;
 for(const auto& stage:value.stages)
  if(stage.active>1||!memchr(stage.phase,0,sizeof(stage.phase))||!memchr(stage.detail,0,sizeof(stage.detail)))return false;
 return true;
}
void commit(){
 // Publish the checksum last. A reset during the preceding writes invalidates
 // the record on the next boot instead of displaying a damaged string.
 std::atomic_signal_fence(std::memory_order_seq_cst);
 retained.checksum=checksum(retained);
}
void copy(char* out,size_t capacity,const char* text){
 if(!text)text="";
 size_t n=0;while(n+1<capacity&&text[n])++n;
 if(text[n])while(n&&(uint8_t(text[n])&0xc0)==0x80)--n;
 memset(out,0,capacity);if(n)memcpy(out,text,n);
}
bool warmReset(esp_reset_reason_t reason){
 switch(reason){
  case ESP_RST_EXT:case ESP_RST_SW:case ESP_RST_PANIC:
  case ESP_RST_INT_WDT:case ESP_RST_TASK_WDT:case ESP_RST_WDT:
  case ESP_RST_DEEPSLEEP:case ESP_RST_SDIO:case ESP_RST_USB:
  case ESP_RST_JTAG:case ESP_RST_CPU_LOCKUP:return true;
  // POWERON/BROWNOUT/PWR_GLITCH/UNKNOWN cannot establish retained context.
  default:return false;
 }
}
void append(const char* text){
 const size_t used=strlen(previous);
 if(used+1<sizeof(previous))snprintf(previous+used,sizeof(previous)-used,"%s",text);
}
}

void tab5DiagnosticsBegin(){
 const bool warm=warmReset(esp_reset_reason());
 portENTER_CRITICAL(&guard);
 previous[0]=0;
 if(warm&&valid(retained)){
  static const char* const labels[]={"Démarrage","Python","Impression","SD","Vidéo","Pentest","Coffre","Git"};
  static_assert(sizeof(labels)/sizeof(labels[0])==Slots,"One label per diagnostic slot");
  for(unsigned i=0;i<Slots;++i){
   const Stage& stage=retained.stages[i];if(!stage.active)continue;
   if(!previous[0])append("Contexte avant redémarrage (cause non établie)\n");
   else append("\n");
   append(labels[i]);append(" : ");append(stage.phase);
   if(stage.detail[0]){append(" — ");append(stage.detail);}
  }
 }
 memset(&retained,0,sizeof(retained));
 retained.magic=Magic;retained.version=Format;retained.size=sizeof(retained);
 commit();ready=true;
 portEXIT_CRITICAL(&guard);
}

extern "C" void tab5DiagnosticMark(unsigned slot,const char* phase,const char* detail,int active){
 if(slot>=Slots)return;
 char phaseCopy[PhaseCapacity],detailCopy[DetailCapacity];
 copy(phaseCopy,sizeof(phaseCopy),phase);copy(detailCopy,sizeof(detailCopy),(slot==VaultSlot||slot==GitSlot)?"":detail);
 portENTER_CRITICAL(&guard);
 if(ready){
  Stage& stage=retained.stages[slot];
  const uint8_t activeValue=active!=0;
  if(stage.active!=activeValue||memcmp(stage.phase,phaseCopy,sizeof(phaseCopy))||memcmp(stage.detail,detailCopy,sizeof(detailCopy))){
   stage.active=activeValue;memcpy(stage.phase,phaseCopy,sizeof(phaseCopy));memcpy(stage.detail,detailCopy,sizeof(detailCopy));
   stage.sequence=++retained.sequence;commit();
  }
 }
 portEXIT_CRITICAL(&guard);
}

const char* tab5DiagnosticPrevious(){return previous;}
