#include "Tab5Serial.h"
#include "Tab5ResourceManager.h"
#include <algorithm>
#include <limits>
#include <stdio.h>
#include <string.h>
#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Tab5TlsMemory.h"
#include "Tab5Localization.h"
#else
#include <thread>
#include <chrono>
#endif
namespace Tab5Resources {
Budget tlsBudget() {
#ifdef ARDUINO
  if(tab5TlsMemoryExternal())return {8*KiB,4*KiB,0,256*KiB,64*KiB};
#endif
  return TlsBudget;
}
Budget radioBudget(bool secure,size_t ringBytes,size_t codecBytes,bool outputRunning) {
  bool internalTls=secure;
#ifdef ARDUINO
  internalTls=secure&&!tab5TlsMemoryExternal();
#endif
  // Net worker 16 KiB, speaker DMA/task/driver ~12 KiB, TCP metadata headroom.
  // An immediate transition can retain I2S; those live allocations are already
  // reflected in free memory. The ring may fall back to 64 KiB if fragmented.
  const size_t internal=(internalTls?76*KiB:40*KiB)-(outputRunning?12*KiB:0);
  const size_t externalTls=secure&&!internalTls?256*KiB:0;
  const size_t psram=ringBytes+1+codecBytes+16*KiB+32*KiB+
      (outputRunning?0:12*KiB)+externalTls;
  return {internal,internalTls?32*KiB:16*KiB,outputRunning?0U:8*KiB,
          psram,std::max(codecBytes,64*KiB+1)};
}
namespace {
size_t sum(size_t a,size_t b) { return b>SIZE_MAX-a ? SIZE_MAX : a+b; }
const char* admissionText(const char* source,char* out,size_t size,bool format=false) {
#ifdef ARDUINO
  if(Tab5Locale.copyText(source,out,size)>=size)return source;
  if(format) {
    // SD language packs may be edited. A format must keep exactly the source
    // argument types/order; a truncated or incompatible one uses the source.
    const char *a=source,*b=out;
    for(;;) {
      a=strchr(a,'%');b=strchr(b,'%');
      if(!a||!b){if(a!=b)return source;break;}
      if((a[1]!='u'&&a[1]!='s')||a[1]!=b[1])return source;
      a+=2;b+=2;
    }
  }
  return out;
#else
  (void)out;(void)size;(void)format;return source;
#endif
}
bool blockShort(MemorySample::Block state,size_t measured,size_t requested) {
  return requested && (state==MemorySample::Block::NoFit ||
      (state==MemorySample::Block::Measured && measured<requested));
}
void describeBlock(char* out,size_t bytes,MemorySample::Block state,size_t measured) {
  const char* text=nullptr;
  switch(state) {
    case MemorySample::Block::Fit:text=">=";break;
    case MemorySample::Block::NoFit:text="<";break;
    case MemorySample::Block::Unknown:text="?";break;
    case MemorySample::Block::Measured:break;
  }
  if(text)snprintf(out,bytes,"%s",text);
  else snprintf(out,bytes,"%u /",unsigned(measured/KiB));
}
uint32_t clockMs() {
#ifdef ARDUINO
  return uint32_t(xTaskGetTickCount()*portTICK_PERIOD_MS);
#else
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
void pauseAdmission() {
#ifdef ARDUINO
  vTaskDelay(1);
#else
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
}
}
Reservation::~Reservation() { release(); }
Reservation::Reservation(Reservation&& other) noexcept
  : _manager(other._manager), _slot(other._slot) { other._manager=nullptr; }
Reservation& Reservation::operator=(Reservation&& other) noexcept {
  if(this!=&other) { release(); _manager=other._manager; _slot=other._slot; other._manager=nullptr; }
  return *this;
}
void Reservation::release() { if(_manager) { _manager->release(_slot); _manager=nullptr; } }
void Reservation::allocated(size_t internalBytes,size_t psramBytes) {
  if(_manager) _manager->allocated(_slot,internalBytes,psramBytes);
}
void Reservation::returned(size_t internalBytes,size_t psramBytes) {
  if(_manager) _manager->returned(_slot,internalBytes,psramBytes);
}
void Manager::allocated(unsigned slot,size_t internalBytes,size_t psramBytes) {
  // Only the owner mutates its slot. Atomic quantities let an admission on
  // the other core observe progress without waiting inside an allocator.
  auto& s=_slots[slot];
  const size_t in=s.internal.load(),ex=s.psram.load();
  s.internal.store(in-std::min(in,internalBytes),std::memory_order_release);
  s.psram.store(ex-std::min(ex,psramBytes),std::memory_order_release);
}
void Manager::returned(unsigned slot,size_t internalBytes,size_t psramBytes) {
  auto& s=_slots[slot];
  const size_t in=s.internal.load(),ex=s.psram.load();
  s.internal.store(in+std::min(s.internalLimit-in,internalBytes),std::memory_order_release);
  s.psram.store(ex+std::min(s.psramLimit-ex,psramBytes),std::memory_order_release);
  // Published before the caller frees heap memory. Admissions reject a sample
  // crossing this transition instead of combining old pending/new free bytes.
  _returnEpoch.fetch_add(1,std::memory_order_release);
}
Denial Manager::rejection(const MemorySample& m,const Budget& b,const Budget& r) {
  // DMA is part of internal RAM, never an additional heap. A task that does
  // not request DMA/PSRAM blocks must not be rejected for their fragmentation.
  if(m.internal<sum(InternalSafety,sum(b.internal,r.internal)))return Denial::Internal;
  if(blockShort(m.internalFit,m.internalBlock,b.internalBlock))return Denial::InternalBlock;
  if(blockShort(m.dmaFit,m.dmaBlock,b.dmaBlock))return Denial::DmaBlock;
  if((b.psram||r.psram)&&m.psram<sum(PsramSafety,sum(b.psram,r.psram)))return Denial::Psram;
  if(blockShort(m.psramFit,m.psramBlock,b.psramBlock))return Denial::PsramBlock;
  return Denial::None;
}
bool Manager::fits(const MemorySample& m,const Budget& b,const Budget& r) {
  return rejection(m,b,r)==Denial::None;
}
void describeAdmission(const AdmissionReport& r,char* out,size_t size) {
  if(!out||!size)return;
  const char* early=nullptr;
  if(r.reason==Denial::AdmissionBusy)early="Admission mémoire occupée : une autre tâche démarre";
  if(r.reason==Denial::SlotsFull)early="Trop de démarrages simultanés : patienter";
  if(r.reason==Denial::CollectionBusy)early="Libération des ressources encore en cours";
  if(early){char translated[192];snprintf(out,size,"%s",admissionText(early,translated,sizeof(translated)));return;}
  const char* cause="Mémoire disponible";
  switch(r.reason){
    case Denial::Internal:cause="RAM interne libre insuffisante";break;
    case Denial::InternalBlock:cause="RAM interne fragmentée";break;
    case Denial::DmaBlock:cause="Bloc DMA libre insuffisant";break;
    case Denial::Psram:cause="PSRAM libre insuffisante";break;
    case Denial::PsramBlock:cause="PSRAM fragmentée";break;
    default:break;
  }
  char internal[24],dma[24],psram[24];
  describeBlock(internal,sizeof(internal),r.memory.internalFit,r.memory.internalBlock);
  describeBlock(dma,sizeof(dma),r.memory.dmaFit,r.memory.dmaBlock);
  describeBlock(psram,sizeof(psram),r.memory.psramFit,r.memory.psramBlock);
  const char* format="%s\nRAM : %u Kio libres ; %u nécessaires (%u démarrage + %u réservés + 32 marge).\nBloc interne : %s %u Kio ; DMA : %s %u Kio.\nPSRAM : %u Kio libres ; %u nécessaires ; bloc %s %u Kio.";
  char translatedFormat[384],translatedCause[128];
  snprintf(out,size,admissionText(format,translatedFormat,sizeof(translatedFormat),true),
      admissionText(cause,translatedCause,sizeof(translatedCause)),unsigned(r.memory.internal/KiB),unsigned(sum(InternalSafety,sum(r.requested.internal,r.pending.internal))/KiB),
      unsigned(r.requested.internal/KiB),unsigned(r.pending.internal/KiB),
      internal,unsigned(r.requested.internalBlock/KiB),
      dma,unsigned(r.requested.dmaBlock/KiB),
      unsigned(r.memory.psram/KiB),unsigned((r.requested.psram||r.pending.psram)?sum(PsramSafety,sum(r.requested.psram,r.pending.psram))/KiB:0),
      psram,unsigned(r.requested.psramBlock/KiB));
}
Budget Manager::pending() const {
  Budget result;
  for(const auto& s:_slots) if(s.used.load(std::memory_order_acquire)) {
    result.internal=sum(result.internal,s.internal.load(std::memory_order_acquire));
    result.psram=sum(result.psram,s.psram.load(std::memory_order_acquire));
  }
  return result;
}
uint8_t Manager::deficientDomains(const Budget& b) {
  // A live operation supplies the exact capabilities it needs. Contention
  // alone never causes eviction and PSRAM cannot replace an SRAM stack.
  if(!lockAdmission())return 0;
  const Budget outstanding=pending();
  const MemorySample m=_admissionSample?_admissionSample(b):_sample();
  uint8_t domains=0;
  if(m.internal<sum(InternalSafety,sum(b.internal,outstanding.internal))||
      blockShort(m.internalFit,m.internalBlock,b.internalBlock)||
      blockShort(m.dmaFit,m.dmaBlock,b.dmaBlock))domains|=uint8_t(MemoryDomain::Internal);
  if(((b.psram||outstanding.psram)&&m.psram<sum(PsramSafety,sum(b.psram,outstanding.psram)))||
      blockShort(m.psramFit,m.psramBlock,b.psramBlock))domains|=uint8_t(MemoryDomain::Psram);
  unlockAdmission();
  return domains;
}
void Manager::begin() {
#ifdef ARDUINO
  // The boot task owns initialization; no scheduler objects in constructors.
  if (!_admissionMutex)
    _admissionMutex = xSemaphoreCreateMutexStatic(&_admissionStorage);
  if (!_collectionEvents)
    _collectionEvents = xEventGroupCreateStatic(&_collectionStorage);
#endif
}
bool Manager::lockAdmission() {
#ifdef ARDUINO
  // Priority inheritance lets a low-priority holder finish when a decoder or
  // network worker requests admission. The former tick polling had none.
  return _admissionMutex &&
      xSemaphoreTake(_admissionMutex, pdMS_TO_TICKS(8)) == pdTRUE;
#else
  return _admissionMutex.try_lock_for(std::chrono::milliseconds(8));
#endif
}
void Manager::unlockAdmission() {
#ifdef ARDUINO
  xSemaphoreGive(_admissionMutex);
#else
  _admissionMutex.unlock();
#endif
}
void Manager::waitForCollection(unsigned index, unsigned timeoutMs) {
  if (!timeoutMs) return;
#ifdef ARDUINO
  // One bit per request: no task notification slot is consumed and no waiter
  // can consume another request's completion. Completion before wait is kept.
  if (_collectionEvents)
    xEventGroupWaitBits(_collectionEvents, EventBits_t(1U << index), pdTRUE,
        pdFALSE, pdMS_TO_TICKS(timeoutMs));
#else
  std::unique_lock<std::mutex> lock(_collectionMutex);
  _collectionChanged.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
    return _requests[index].state.load(std::memory_order_acquire) == Done;
  });
#endif
}
void Manager::collectionFinished(unsigned index) {
#ifdef ARDUINO
  if (_collectionEvents)
    xEventGroupSetBits(_collectionEvents, EventBits_t(1U << index));
#else
  // Pair the predicate check/wait with notification, including a completion
  // that arrives exactly as the requester starts sleeping.
  std::lock_guard<std::mutex> lock(_collectionMutex);
  _collectionChanged.notify_all();
  (void)index;
#endif
}
Reservation Manager::reserve(Owner owner,const Budget& budget,AdmissionReport* report) {
  return admit(owner,budget,report,true);
}
Reservation Manager::admit(Owner,const Budget& budget,AdmissionReport* report,bool countFailure) {
  if(report){*report={};report->requested=budget;
    report->memory.internalFit=report->memory.dmaFit=report->memory.psramFit=MemorySample::Block::Unknown;}
  if(!lockAdmission()) {
    if(report)report->reason=Denial::AdmissionBusy;
    if(countFailure)++_denied;
    return {};
  }
  int slot=-1;
  const uint32_t beforeReturn=_returnEpoch.load(std::memory_order_acquire);
  const Budget outstanding=pending();
  // Counters can reject capacity pressure before querying contiguous fit.
  if(!countFailure) {
    const MemoryTotals totals=_totals();
    const bool internalShort=totals.internal<sum(InternalSafety,sum(budget.internal,outstanding.internal));
    const bool externalShort=(budget.psram||outstanding.psram)&&
        totals.psram<sum(PsramSafety,sum(budget.psram,outstanding.psram));
    if(internalShort||externalShort) {
      if(report){report->memory.internal=totals.internal;report->memory.psram=totals.psram;
        report->pending=outstanding;report->reason=internalShort?Denial::Internal:Denial::Psram;}
      unlockAdmission();
      return {};
    }
  }
  // Both the first attempt and the final refusal use the bounded size query.
  // Full largest-block diagnostics are explicit, never an admission side effect.
  const MemorySample memory=_admissionSample?_admissionSample(budget):_sample();
  const Denial reason=beforeReturn!=_returnEpoch.load(std::memory_order_acquire)?
      Denial::AdmissionBusy:rejection(memory,budget,outstanding);
  if(reason==Denial::None) {
    for(unsigned i=0;i<MaxReservations;++i) if(!_slots[i].used.load(std::memory_order_acquire)) { slot=i; break; }
  }
  if(slot>=0) {
    _slots[slot].internalLimit=budget.internal;
    _slots[slot].psramLimit=budget.psram;
    _slots[slot].internal.store(budget.internal);
    _slots[slot].psram.store(budget.psram);
    _slots[slot].used.store(true,std::memory_order_release);
  }
  if(report){report->memory=memory;report->pending=outstanding;
    report->reason=slot>=0?Denial::None:reason==Denial::None?Denial::SlotsFull:reason;}
  unlockAdmission();
  if(slot<0) { if(countFailure)++_denied; return {}; }
  return Reservation(this,unsigned(slot));
}
int Manager::queueCollection(Owner owner,const Budget& budget) {
  for(unsigned i=0;i<MaxReservations;++i) {
    auto& r=_requests[i];uint8_t state=Free;
    if(!r.state.compare_exchange_strong(state,Building,std::memory_order_acq_rel))continue;
#ifdef ARDUINO
    if (_collectionEvents) xEventGroupClearBits(_collectionEvents, EventBits_t(1U << i));
#endif
    r.budget=budget;r.owner=owner;
    r.state.store(Pending,std::memory_order_release);
    return int(i);
  }
  return -1;
}
bool Manager::finishRequest(unsigned index) {
  auto& r=_requests[index];auto state=r.state.load(std::memory_order_acquire);
  for(;;) {
    // A callback may currently be running. Its slot remains owned by the
    // collector until it returns; a timed-out worker cannot recycle it.
    const uint8_t next=state==Processing?Abandoned:Free;
    if(state==Free||state==Abandoned)return state==Free;
    if(r.state.compare_exchange_weak(state,next,std::memory_order_acq_rel))return next==Free;
  }
}
void Manager::serviceCollections() {
  for(unsigned i=0;i<MaxReservations;++i) {
    auto& r=_requests[i];
    uint8_t state=Pending;
    if(!r.state.compare_exchange_strong(state,Processing,std::memory_order_acq_rel))continue;
    // Copy before invoking user code. The waiter can abandon but cannot reuse
    // this slot until completion, so neither field can change under us.
    const Budget budget=r.budget;const Owner owner=r.owner;
    collect(budget,owner);
    state=Processing;
    if(r.state.compare_exchange_strong(state,Done,std::memory_order_acq_rel))
      collectionFinished(i);
    else
      r.state.store(Free,std::memory_order_release); // abandoned by its waiter
  }
}
Reservation Manager::reserveRecovering(Owner owner,const Budget& budget,bool collectorThread,
    AdmissionReport* report,unsigned timeoutMs) {
  if(collectorThread) {
    auto admission=admit(owner,budget,report,false);
    if(admission)return admission;
    collect(budget,owner);
    admission=admit(owner,budget,report,false);
    const uint32_t started=clockMs();
    // A retired worker's stack may still await FreeRTOS idle cleanup. Give
    // idle tasks a short window without nesting LVGL/cache service callbacks
    // from inside the button event that requested this launch.
    const unsigned idleWaitMs=std::min(timeoutMs,60U);
    while(!admission&&uint32_t(clockMs()-started)<idleWaitMs) {
      pauseAdmission();
      admission=admit(owner,budget,report,false);
    }
    if(admission)return admission;
    return reserve(owner,budget,report);
  }
  auto admission=admit(owner,budget,report,false);
  if(admission)return admission;
  const uint32_t started=clockMs();int request=-1;bool collected=false;
  while(uint32_t(clockMs()-started)<timeoutMs) {
    if(!collected&&request<0)request=queueCollection(owner,budget);
    if(request>=0&&_requests[request].state.load(std::memory_order_acquire)==Done) {
      finishRequest(unsigned(request));request=-1;collected=true;
    }
    if(request>=0 && !collected) {
      const uint32_t elapsed=uint32_t(clockMs()-started);
      if(elapsed<timeoutMs)waitForCollection(unsigned(request),timeoutMs-elapsed);
      continue;
    }
    // Do not start a new workload concurrently with a cache callback. Once
    // collection has finished, allow retired tasks' stacks to be reclaimed by
    // FreeRTOS idle tasks, or another startup reservation to be released.
    if(collected) {
      admission=admit(owner,budget,report,false);
      if(admission)return admission;
    }
    pauseAdmission();
  }
  if(request>=0&&!finishRequest(unsigned(request))) {
    // The UI callback is still executing. It is unsafe to start even if an
    // intermediate heap sample already looks sufficient.
    if(report)report->reason=Denial::CollectionBusy;
    ++_denied;return {};
  }
  // Exactly one final refusal is counted/reported, never one per retry.
  return reserve(owner,budget,report);
}
bool Manager::addCache(Owner owner,const char* name,Reclaim callback,void* context,CachePolicy policy) {
  if(!callback||!uint8_t(policy.domains)||_cacheCount>=MaxCaches) return false;
  auto& cache=_caches[_cacheCount++];
  cache.owner=owner;cache.name=name;cache.reclaim=callback;cache.context=context;
  cache.lastUse=++_clock;cache.policy=policy;
  return true;
}
void Manager::foreground(Owner owner) {
  if(_foreground==owner)return;
  for(unsigned i=0;i<_cacheCount;++i)
    if(_caches[i].owner==owner)_caches[i].lastUse=++_clock;
  _foreground=owner;
}
void Manager::collect(const Budget& budget,Owner requester) {
  collectImpl(budget,requester);
}
void Manager::collectImpl(const Budget& budget,Owner requester) {
  if(_collecting)return;
  uint8_t needed=deficientDomains(budget);
  if(!needed)return;
  _collecting=true; ++_collections;
  uint32_t visited=0;
  for(unsigned n=0;n<_cacheCount&&needed;++n) {
    int selected=-1;
    for(unsigned i=0;i<_cacheCount;++i) {
      if(visited&(1u<<i)) continue;
      const auto& c=_caches[i];
      if(!(uint8_t(c.policy.domains)&needed))continue;
      // Visible and currently requesting applications own live state. The OS
      // must not call their destructors, even if a callback forgot its veto.
      // Safe app-local trimming belongs to explicit close/finish transitions.
      if(c.owner==_foreground||c.owner==requester)continue;
      if(selected<0||c.lastUse<_caches[selected].lastUse)selected=i;
    }
    if(selected<0) break;
    visited|=1u<<selected;
    reclaim(_caches[selected]);
    needed=deficientDomains(budget);
  }
  _collecting=false;
}
void Manager::reclaim(Cache& c) {
  const MemoryTotals before=_totals();
  c.reclaim(c.context);
  const MemoryTotals after=_totals();
  // Callback estimates can include PSRAM or deferred task deletion. Only
  // actual heap gains are counted, with capabilities kept separate.
  const size_t internal=after.internal>before.internal?after.internal-before.internal:0;
  const size_t psram=after.psram>before.psram?after.psram-before.psram:0;
  _reclaimedInternal.fetch_add(internal);_reclaimedPsram.fetch_add(psram);
  _reclaimed.fetch_add(sum(internal,psram));
}
} // namespace Tab5Resources

#ifdef ARDUINO
#include "Tab5Memory.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
Tab5Resources::Manager Tab5MemoryManager(tab5MemorySample,tab5MemoryTotals,tab5MemorySample);
namespace {
TaskHandle_t uiTask=nullptr;
std::atomic<size_t> failedBytes{0};std::atomic<uint32_t> failedCaps{0},failedCount{0};
void allocationFailed(size_t bytes,uint32_t caps,const char*) {
  // Called from the allocator: no logging, heap call, mutex or eviction here.
  failedBytes.store(bytes);failedCaps.store(caps);failedCount.fetch_add(1);
}
}
void tab5ResourcesBegin() {
  uiTask=xTaskGetCurrentTaskHandle();
  Tab5MemoryManager.begin();
  tab5TaskDiagnosticsBegin();tab5TaskStarted("ui");
  heap_caps_register_failed_alloc_callback(allocationFailed);
#if defined(CONFIG_SPIRAM_USE_MALLOC) && CONFIG_SPIRAM_USE_MALLOC
  // All generic data allocations prefer PSRAM, including short strings and
  // catalogue nodes. Drivers requiring INTERNAL/DMA and FreeRTOS task stacks
  // already request their capabilities explicitly and are unaffected. There
  // is no reason to consume scarce SRAM just because a text entry is small.
  heap_caps_malloc_extmem_enable(0);
#endif
  tab5LogMemory("gestionnaire global actif");
}
void tab5ResourcesService() {
  if(xTaskGetCurrentTaskHandle()!=uiTask) return;
  Tab5MemoryManager.serviceCollections();
  const uint32_t failures=failedCount.exchange(0);
  const size_t bytes=failedBytes.load();const uint32_t caps=failedCaps.load();
  if(failures)Tab5DebugSerial.printf("[MEM][ALLOC] %u echec(s), dernier=%u octets caps=0x%lx\n",
      unsigned(failures),unsigned(bytes),(unsigned long)caps);
  // A low-level allocation failure is diagnostic only: its caller may have
  // retried, reduced its optional cache, or cancelled before this UI turn.
  // Never turn that stale notification into an unrelated asynchronous purge.
  // Operations needing recovery use reserveRecovering with their live budget.

}
Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner owner,
    const Tab5Resources::Budget& budget,const char* operation,
    Tab5Resources::AdmissionReport* report) {
  Tab5Resources::AdmissionReport local;
  auto reservation=Tab5MemoryManager.reserveRecovering(owner,budget,
      xTaskGetCurrentTaskHandle()==uiTask,report?report:&local);
  if(!reservation) {
    char reason[512];Tab5Resources::describeAdmission(report?*report:local,reason,sizeof(reason));
    Tab5DebugSerial.printf("[MEM][REFUS] %s | %s\n",operation,reason);
    tab5LogMemory("operation non demarree");
  }
  return reservation;
}
#endif
