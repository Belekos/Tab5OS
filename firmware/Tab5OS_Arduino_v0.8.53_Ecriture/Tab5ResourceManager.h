#pragma once
#include "Tab5ResourcePolicy.h"
#include <atomic>
#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#else
#include <mutex>
#include <condition_variable>
#endif

// OS-wide admission and cache reclamation. No private heap, swap, or forced
// task deletion. Only explicitly reloadable data may register a reclaimer.
namespace Tab5Resources {
enum class Owner : uint8_t { System, Files, Map, Audio, Python, Writing, Reader, Video, Tools, Image };
struct Budget {
  size_t internal = 0, internalBlock = 0, dmaBlock = 0;
  size_t psram = 0, psramBlock = 0;
};
// Capability hints describe what a callback can actually release. PSRAM is
// not interchangeable with SRAM: dropping map tiles cannot create a task stack.
enum class MemoryDomain : uint8_t { Internal = 1, Psram = 2, Any = 3 };
struct CachePolicy {
  MemoryDomain domains = MemoryDomain::Any;
};
constexpr Budget TaskBudget(size_t stack) { return {stack + 4*KiB, stack, 0, 0, 0}; }
constexpr Budget PythonBudget{36*KiB,32*KiB,0,2200*KiB,2*MiB};
constexpr Budget TlsBudget{48*KiB,32*KiB,0,128*KiB,32*KiB};
// Match the allocator actually installed at boot. TLS scratch can reside in
// PSRAM; task stacks, TCP control state and I2S DMA remain separate charges.
Budget tlsBudget();
Budget radioBudget(bool secure, size_t ringBytes, size_t codecBytes,
                   bool outputRunning);
constexpr size_t InternalSafety = 32*KiB, PsramSafety = 512*KiB;
enum class Denial : uint8_t {
  None, AdmissionBusy, SlotsFull, Internal, InternalBlock, DmaBlock, Psram, PsramBlock,
  CollectionBusy
};
// A caller-owned snapshot: a second core cannot overwrite the reason shown to
// the user. Pending bytes exclude allocations already reported to the manager.
struct AdmissionReport {
  Denial reason = Denial::None;
  MemorySample memory{};
  Budget requested{}, pending{};
};
void describeAdmission(const AdmissionReport& report, char* out, size_t size);

class Manager;
class Reservation {
 public:
  Reservation() = default;
  ~Reservation();
  Reservation(Reservation&& other) noexcept;
  Reservation& operator=(Reservation&& other) noexcept;
  Reservation(const Reservation&) = delete;
  Reservation& operator=(const Reservation&) = delete;
  explicit operator bool() const { return _manager != nullptr; }
  // Call AFTER a successful allocation. Heap samples already include those
  // bytes; only the remaining startup peak must stay reserved.
  void allocated(size_t internalBytes, size_t psramBytes);
  // Restore a still-live quota's future budget BEFORE returning these bytes
  // to the heap. For a failed shrink, undo this with allocated(). Only the
  // owning task may mutate its reservation; values clamp to the initial budget.
  void returned(size_t internalBytes, size_t psramBytes);
  void release();
 private:
  friend class Manager;
  Reservation(Manager* manager, unsigned slot) : _manager(manager), _slot(slot) {}
  Manager* _manager = nullptr;
  unsigned _slot = 0;
};

class Manager {
 public:
  using Sample = MemorySample (*)();
  using TotalsSample = MemoryTotals (*)();
  using AdmissionSample = MemorySample (*)(const Budget&);
  using Reclaim = size_t (*)(void*);
  Manager(Sample sample, TotalsSample totals, AdmissionSample admissionSample = nullptr)
      : _sample(sample), _totals(totals), _admissionSample(admissionSample) {}
  // Call once on the boot task before publishing the manager to workers.
  // Native kernel objects use embedded internal storage, never allocator fallback.
  void begin();
  // Registration, foreground and collection: UI task only, outside rendering.
  bool addCache(Owner owner, const char* name, Reclaim reclaim, void* context = nullptr,
      CachePolicy policy = {});
  void foreground(Owner owner);
  void collect(const Budget& budget = {}, Owner requester = Owner::System);
  // Thread safe; bounded lock wait. Reservations cover future startup bytes.
  Reservation reserve(Owner owner, const Budget& budget, AdmissionReport* report = nullptr);
  // Recover within the original operation. A worker queues the exact budget;
  // only the UI/collector executes cache destructors. Waiting is bounded and
  // yields the CPU, including when the UI cannot service a request immediately.
  Reservation reserveRecovering(Owner owner, const Budget& budget, bool collectorThread,
      AdmissionReport* report = nullptr, unsigned timeoutMs = 1500);
  void serviceCollections(); // UI/collector only; serves live operation requests.
  // Cache reclamation has no timer or idle expiry. A failed admission requests
  // the exact operation budget; foreground/requester caches are never evicted.
  unsigned denied() const { return _denied.load(); }
  size_t reclaimed() const { return _reclaimed.load(); }
  size_t reclaimedInternal() const { return _reclaimedInternal.load(); }
  size_t reclaimedPsram() const { return _reclaimedPsram.load(); }
  unsigned collections() const { return _collections.load(); }
  static bool fits(const MemorySample&, const Budget&, const Budget& reserved = {});
  static Denial rejection(const MemorySample&, const Budget&, const Budget& reserved = {});
 private:
  friend class Reservation;
  static constexpr unsigned MaxCaches = 16, MaxReservations = 8;
  struct Cache { Owner owner{}; const char* name=nullptr; Reclaim reclaim=nullptr;
    void* context=nullptr; uint32_t lastUse=0; CachePolicy policy{}; };
  struct Slot { std::atomic<size_t> internal{0}, psram{0};
    size_t internalLimit=0, psramLimit=0;
    std::atomic<bool> used{false}; };
  enum RequestState : uint8_t { Free, Building, Pending, Processing, Done, Abandoned };
  struct Request { std::atomic<uint8_t> state{Free}; Budget budget{}; Owner owner{}; };
  void release(unsigned slot) { _slots[slot].used.store(false, std::memory_order_release); }
  void allocated(unsigned slot, size_t internalBytes, size_t psramBytes);
  void returned(unsigned slot, size_t internalBytes, size_t psramBytes);
  uint8_t deficientDomains(const Budget&);
  void collectImpl(const Budget&, Owner);
  void reclaim(Cache&);
  bool lockAdmission();
  void unlockAdmission();
  void waitForCollection(unsigned index, unsigned timeoutMs);
  void collectionFinished(unsigned index);
  Reservation admit(Owner owner, const Budget& budget, AdmissionReport* report, bool countFailure);
  int queueCollection(Owner owner, const Budget& budget);
  bool finishRequest(unsigned index);
  Budget pending() const;
  Sample _sample;
  TotalsSample _totals;
  AdmissionSample _admissionSample;
  Cache _caches[MaxCaches]{};
  Slot _slots[MaxReservations]{};
  Request _requests[MaxReservations]{};
  unsigned _cacheCount=0;
  Owner _foreground=Owner::System;
  uint32_t _clock=0;
  bool _collecting=false;
#ifdef ARDUINO
  StaticSemaphore_t _admissionStorage{};
  SemaphoreHandle_t _admissionMutex = nullptr;
  StaticEventGroup_t _collectionStorage{};
  EventGroupHandle_t _collectionEvents = nullptr;
#else
  std::timed_mutex _admissionMutex;
  std::mutex _collectionMutex;
  std::condition_variable _collectionChanged;
#endif
  std::atomic<uint32_t> _returnEpoch{0};
  std::atomic<unsigned> _denied{0}, _collections{0};
  std::atomic<size_t> _reclaimed{0};
  std::atomic<size_t> _reclaimedInternal{0}, _reclaimedPsram{0};
};
} // namespace Tab5Resources

extern Tab5Resources::Manager Tab5MemoryManager;
void tab5ResourcesBegin();
void tab5ResourcesService();
Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner owner,
    const Tab5Resources::Budget& budget, const char* operation,
    Tab5Resources::AdmissionReport* report = nullptr);
