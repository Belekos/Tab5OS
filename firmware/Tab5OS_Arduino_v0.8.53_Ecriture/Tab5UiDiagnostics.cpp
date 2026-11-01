#include "Tab5UiDiagnostics.h"
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <new>
#include <cstring>
#include <cstdio>
#include <cstddef>
#include <atomic>

namespace Tab5UiDiagnostics { namespace {
constexpr uint32_t Magic=0x35554944,SlowUs=20000;
constexpr unsigned Entries=8;
struct Event {uint32_t atMs,durationUs;uint8_t phase,reserved[3];};
struct Journal {uint32_t magic;uint16_t count,next;Event events[Entries];uint32_t checksum;};
static_assert(2*sizeof(Journal)<=256,"Keep retained UI diagnostics bounded");
// No initializer: retain only across reset reasons for which RTC is meaningful.
RTC_NOINIT_ATTR Journal currentJournal;
RTC_NOINIT_ATTR Journal previousJournal;
struct Runtime {
  Stats stats;
  uint64_t lastEvent[Count]{};
  uint64_t loopStart=0,previousLoop=0;
  uint32_t loopGeneration=0;
  bool haveLoop=false;
};
// Optional counters are cold data, not control state required by drivers/ISRs.
// Retain them after disabling for inspection; begin() releases them at reset.
Runtime* runtime=nullptr;
bool ready=false;
uint32_t generation=0;
uint32_t clamp(uint64_t n){return n>UINT32_MAX?UINT32_MAX:uint32_t(n);}
uint32_t checksum(const Journal& j){
  uint32_t h=2166136261u;auto* p=reinterpret_cast<const uint8_t*>(&j);
  for(size_t i=0;i<offsetof(Journal,checksum);++i)h=(h^p[i])*16777619u;
  return h;
}
bool valid(const Journal& j){
  if(j.magic!=Magic||j.count>Entries||j.next>=Entries||j.checksum!=checksum(j))return false;
  for(unsigned i=0;i<j.count;++i)if(j.events[i].phase>=Count)return false;
  return true;
}
void seal(Journal& j){std::atomic_signal_fence(std::memory_order_seq_cst);j.checksum=checksum(j);}
void clear(Journal& j){memset(&j,0,sizeof(j));j.magic=Magic;seal(j);}
bool warm(esp_reset_reason_t r){
  switch(r){case ESP_RST_EXT:case ESP_RST_SW:case ESP_RST_PANIC:case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:case ESP_RST_WDT:case ESP_RST_DEEPSLEEP:case ESP_RST_SDIO:
    case ESP_RST_USB:case ESP_RST_JTAG:case ESP_RST_CPU_LOCKUP:return true;
    default:return false;}
}
void record(unsigned phase,uint64_t elapsed,uint64_t now){
  if(!runtime||phase>=Count)return;
  auto& p=runtime->stats.phases[phase];++p.calls;p.totalUs+=elapsed;
  const uint32_t duration=clamp(elapsed);if(duration>p.maxUs)p.maxUs=duration;
  if(duration<SlowUs)return;
  ++p.slow;
  // Keep counts for every slow sample, but at most one RTC event/phase/second.
  if(!ready||(runtime->lastEvent[phase]&&now-runtime->lastEvent[phase]<1000000))return;
  runtime->lastEvent[phase]=now;
  auto& e=currentJournal.events[currentJournal.next];
  e={uint32_t(now/1000),duration,uint8_t(phase),{0,0,0}};
  currentJournal.next=(currentJournal.next+1)%Entries;
  if(currentJournal.count<Entries)++currentJournal.count;
  seal(currentJournal);
}
}
void begin(){
  if(warm(esp_reset_reason())&&valid(currentJournal))previousJournal=currentJournal;
  else clear(previousJournal);
  clear(currentJournal);++generation;ready=true;
  if(runtime){runtime->~Runtime();heap_caps_free(runtime);runtime=nullptr;}
}
bool enabled(){return runtime&&runtime->stats.enabled;}
bool enable(bool value){
  if(value==enabled())return true;
  if(!runtime){
    auto* storage=heap_caps_malloc(sizeof(Runtime),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!storage)return false;
    runtime=new(storage)Runtime;
  }
  ++generation;runtime->haveLoop=false;
  if(value){runtime->stats={};clear(currentJournal);memset(runtime->lastEvent,0,sizeof(runtime->lastEvent));}
  runtime->stats.enabled=value;
  return true;
}
Stats snapshot(){return runtime?runtime->stats:Stats{};}
const char* phaseName(unsigned phase){
  if(phase<Tab5DisplayActivity::sourceCount)
    return Tab5DisplayActivity::sourceName(Tab5DisplayActivity::Source(phase));
  switch(phase){case Input:return "Entrées";case Services:return "Applications";
    case Persistence:return "Enregistrement";case Loop:return "Interface";default:return "inconnue";}
}
Scope::Scope(unsigned phase):phase_(phase),active_(enabled()){
  if(active_){generation_=generation;started_=uint64_t(esp_timer_get_time());}
}
Scope::~Scope(){
  if(!active_||!enabled()||generation_!=generation)return;
  const uint64_t now=uint64_t(esp_timer_get_time());record(phase_,now-started_,now);
}
void loopBegin(){
  if(!enabled()){if(runtime)runtime->haveLoop=false;return;}
  const uint64_t now=uint64_t(esp_timer_get_time());
  if(runtime->haveLoop){const uint32_t gap=clamp(now-runtime->previousLoop);if(gap>runtime->stats.maxGapUs)runtime->stats.maxGapUs=gap;}
  runtime->previousLoop=runtime->loopStart=now;runtime->loopGeneration=generation;runtime->haveLoop=true;
}
void loopEnd(){
  if(!enabled()||!runtime->haveLoop||runtime->loopGeneration!=generation)return;
  const uint64_t now=uint64_t(esp_timer_get_time());record(Loop,now-runtime->loopStart,now);
}
size_t recent(char* out,size_t cap,bool previousBoot){
  if(!out||!cap)return 0;out[0]=0;
  const auto& j=previousBoot?previousJournal:currentJournal;
  if(!valid(j))return 0;
  size_t used=0;
  for(unsigned n=0;n<j.count;++n){
    const auto& e=j.events[(j.next+Entries-j.count+n)%Entries];
    char line[128];
    int written=snprintf(line,sizeof(line),"%s%lu ms | %s | %lu us",n?"\n":"",
        static_cast<unsigned long>(e.atMs),phaseName(e.phase),static_cast<unsigned long>(e.durationUs));
    // Append whole lines only: even tiny callers never receive split UTF-8.
    if(written<0||size_t(written)>=sizeof(line)||size_t(written)>=cap-used)break;
    memcpy(out+used,line,size_t(written)+1);used+=size_t(written);
  }
  return used;
}
}
