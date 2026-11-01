#include "Tab5Serial.h"
#include "Tab5Memory.h"
#include "Tab5MemoryLayout.h"
#include "Tab5ResourceManager.h"
#include "Tab5HeapProbe.h"
#include "Tab5LvglMemory.h"
#include <Arduino.h>
#ifdef ARDUINO
#include "Tab5ResourceManager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include <string.h>
#include <cstdio>
#include <algorithm>
#ifdef ARDUINO
namespace {
// Names in FreeRTOS snapshots point into live TCBs and may become invalid as
// soon as the scheduler resumes. Keep our own small, PSRAM-owned name registry;
// unknown SDK tasks still report their stable numeric task id and real counters.
constexpr unsigned TaskNames=32,TaskLimit=128;
struct TaskName {UBaseType_t id=0;char name[32]{};};
TaskName* taskNames=nullptr;unsigned nextTaskName=0;
portMUX_TYPE taskNamesGuard=portMUX_INITIALIZER_UNLOCKED;
}
#endif
void tab5TaskDiagnosticsBegin(){
#ifdef ARDUINO
  if(!taskNames)taskNames=static_cast<TaskName*>(heap_caps_calloc(TaskNames,sizeof(TaskName),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#endif
}
void tab5TaskStarted(const char* name){
#ifdef ARDUINO
  if(!taskNames)return;
  TaskStatus_t own{};vTaskGetInfo(nullptr,&own,pdFALSE,eInvalid);
  const UBaseType_t id=own.xTaskNumber;
  portENTER_CRITICAL(&taskNamesGuard);
  auto& row=taskNames[nextTaskName++%TaskNames];row.id=id;
  size_t n=0;if(name)while(n+1<sizeof(row.name)&&name[n])++n;
  memcpy(row.name,name?name:"",n);row.name[n]=0;
  portEXIT_CRITICAL(&taskNamesGuard);
#else
  (void)name;
#endif
}
bool tab5DumpTasks(void (*emit)(const char*,size_t,void*),void* context){
#ifdef ARDUINO
  if(!emit)return false;
  // Capacity includes a little room for tasks created while allocating. Never
  // allocate a VLA on the terminal's scarce internal stack.
  const unsigned count=uxTaskGetNumberOfTasks();
  if(count>TaskLimit)return false;
  const unsigned capacity=std::min(TaskLimit,count+8U);
  auto* rows=static_cast<TaskStatus_t*>(heap_caps_calloc(capacity,sizeof(TaskStatus_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!rows)return false;
  configRUN_TIME_COUNTER_TYPE total=0;
  const unsigned used=uxTaskGetSystemState(rows,capacity,&total);
  if(!used){heap_caps_free(rows);return false;}
  char line[192];
  int length=snprintf(line,sizeof(line),"tasks=%u runtime=%llu\nid name state priority stack_min_bytes runtime\n",used,static_cast<unsigned long long>(total));
  emit(line,size_t(length),context);
  for(unsigned i=0;i<used;++i){
    char name[32]="sdk";
    if(taskNames){portENTER_CRITICAL(&taskNamesGuard);
      for(unsigned n=0;n<TaskNames;++n)if(taskNames[n].id==rows[i].xTaskNumber){memcpy(name,taskNames[n].name,sizeof(name));break;}
      portEXIT_CRITICAL(&taskNamesGuard);}
    // Only value fields are used after snapshot: never dereference pxTaskName,
    // pxStackBase or xHandle while an idle task may reclaim their storage.
    length=snprintf(line,sizeof(line),"%u %s %u %u %u %llu\n",unsigned(rows[i].xTaskNumber),name,
        unsigned(rows[i].eCurrentState),unsigned(rows[i].uxCurrentPriority),unsigned(rows[i].usStackHighWaterMark),
        static_cast<unsigned long long>(rows[i].ulRunTimeCounter));
    emit(line,size_t(length),context);
  }
  heap_caps_free(rows);return true;
#else
  (void)emit;(void)context;return false;
#endif
}
bool tab5DumpMemory(void (*emit)(const char*,size_t,void*),void* context) {
  if(!emit)return false;
  // The largest-block queries are deliberately confined to this explicit
  // command, not a periodic sample, task-start registry, or allocator hook.
  const auto memory=Tab5MemoryLayout::sample(true);
  char line[240];
  auto send=[&](int length){if(length>0)emit(line,std::min(size_t(length),sizeof(line)-1),context);};
  send(snprintf(line,sizeof(line),"memory bytes: total free minimum_free largest_block\n"));
  auto row=[&](const char* name,const Tab5MemoryLayout::Pool& p){
    send(snprintf(line,sizeof(line),"%s %llu %llu %llu %llu\n",name,
        static_cast<unsigned long long>(p.total),static_cast<unsigned long long>(p.free),
        static_cast<unsigned long long>(p.minimumFree),static_cast<unsigned long long>(p.largestFree)));
  };
  row("INTERNAL aggregate",memory.internal);
  if(memory.splitAvailable){
    row("HP SRAM heap",memory.hp);row("SPM/TCM heap",memory.tcm);row("LP SRAM heap",memory.lp);
    const size_t reserved=memory.hpPhysicalBytes>memory.l2CacheBytes+memory.hp.total?
        memory.hpPhysicalBytes-memory.l2CacheBytes-memory.hp.total:0;
    send(snprintf(line,sizeof(line),"physical HP=%llu SPM/TCM=%llu LP=%llu L2_cache=%llu HP_code_data_reserved=%llu\n",
        static_cast<unsigned long long>(memory.hpPhysicalBytes),static_cast<unsigned long long>(memory.tcmPhysicalBytes),
        static_cast<unsigned long long>(memory.lpPhysicalBytes),static_cast<unsigned long long>(memory.l2CacheBytes),
        static_cast<unsigned long long>(reserved)));
  }
  row("PSRAM heap",memory.psram);
  const auto lvgl=tab5LvglMemoryStats();
  send(snprintf(line,sizeof(line),"LVGL private: backing_bytes=%llu used_bytes=%llu live_blocks=%llu pools=%llu peak_used=%llu\n",
      static_cast<unsigned long long>(lvgl.ownedBytes),static_cast<unsigned long long>(lvgl.usedBytes),
      static_cast<unsigned long long>(lvgl.liveBlocks),static_cast<unsigned long long>(lvgl.pools),
      static_cast<unsigned long long>(lvgl.peakUsedBytes)));
  send(snprintf(line,sizeof(line),"LVGL backing is already included in PSRAM usage; unused pool bytes are reusable, not additional physical RAM\n"));
  send(snprintf(line,sizeof(line),"minimum_free sums each registered region's low-water mark; INTERNAL includes HP+SPM/TCM+LP, not additional memory\n"));
  return true;
}
Tab5Resources::MemoryTotals tab5MemoryTotals() {
  // Counter reads only: largest-block queries walk the heap with IRQs masked.
  return {heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
          heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)};
}
Tab5Resources::MemorySample tab5MemorySample() {
  const auto in = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  const auto ex = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  return {heap_caps_get_free_size(in), heap_caps_get_largest_free_block(in),
          heap_caps_get_largest_free_block(in | MALLOC_CAP_DMA),
          heap_caps_get_free_size(ex), heap_caps_get_largest_free_block(ex)};
}
Tab5Resources::MemorySample tab5MemorySample(const Tab5Resources::Budget& requested) {
  const auto in=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
  const auto ex=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
  Tab5Resources::MemorySample result;
  result.internal=heap_caps_get_free_size(in);
  result.psram=heap_caps_get_free_size(ex);
  auto probe=[](size_t bytes,uint32_t caps,size_t& largest,
                Tab5Resources::MemorySample::Block& state) {
    using Block=Tab5Resources::MemorySample::Block;
    if(!bytes){state=Block::Unknown;return;}
    const int fit=tab5HeapCanAllocate(bytes,caps);
    if(fit==TAB5_HEAP_FIT_YES)state=Block::Fit;
    else if(fit==TAB5_HEAP_FIT_NO)state=Block::NoFit;
    else {
      // Unsupported SDK/configuration: retain native behaviour. Do not infer
      // incompatibility or report an unmeasured maximum of zero.
      largest=heap_caps_get_largest_free_block(caps);state=Block::Measured;
    }
  };
  probe(requested.internalBlock,in,result.internalBlock,result.internalFit);
  probe(requested.dmaBlock,in|MALLOC_CAP_DMA,result.dmaBlock,result.dmaFit);
  probe(requested.psramBlock,ex,result.psramBlock,result.psramFit);
  return result;
}

bool Tab5ByteBuffer::reserve(size_t bytes) {
  if (_detached) return false;
  if (bytes > _limit) { _overflowed = true; return false; }
  if (bytes <= _capacity) return true;
  void* resized = heap_caps_realloc(_data, bytes,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!resized) { _exhausted = true; return false; }
  _data = static_cast<uint8_t*>(resized);
  _capacity = bytes;
  return true;
}

bool Tab5ByteBuffer::append(const uint8_t* source, size_t length) {
  if (_detached) return false;
  if (!length) return true;
  if (!source) return false;
  if (length > _limit - _size) { _overflowed = true; return false; }
  const size_t needed = _size + length;
  if (needed > _capacity && !reserve(Tab5Resources::growthCapacity(
          _capacity, needed, _limit))) return false;
  memcpy(_data + _size, source, length);
  _size = needed;
  return true;
}

uint8_t* Tab5ByteBuffer::detach() {
  // Keep size() as response metadata, but relinquish ownership exactly once.
  uint8_t* data = _data;
  _data = nullptr;
  _capacity = 0;
  _detached = true;
  return data;
}

void tab5LogMemory(const char* stage) {
  // Automatic lifecycle logs use counters only. Heap walks and stack scans
  // belong to explicit diagnostics, never a decoder/UI transition.
  const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  const uint32_t external = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  Tab5DebugSerial.printf("[MEM] %s | interne=%u Kio min=%u Kio | "
                "PSRAM=%u Kio min=%u Kio | DMA interne=%u Kio | caches liberes=%u Kio refus=%u\n", stage,
      unsigned(heap_caps_get_free_size(internal) / 1024U),
      unsigned(heap_caps_get_minimum_free_size(internal) / 1024U),
      unsigned(heap_caps_get_free_size(external) / 1024U),
      unsigned(heap_caps_get_minimum_free_size(external) / 1024U),
      unsigned(heap_caps_get_free_size(internal | MALLOC_CAP_DMA) / 1024U),
      unsigned(Tab5MemoryManager.reclaimed()/1024U),Tab5MemoryManager.denied());
}
