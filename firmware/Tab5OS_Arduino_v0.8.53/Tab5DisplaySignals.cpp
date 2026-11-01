#include "Tab5DisplaySignals.h"
#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include <atomic>
#include <esp_attr.h>
#include <esp_idf_version.h>
#include <esp_intr_alloc.h>
#include <esp_timer.h>
#include <hal/mipi_dsi_brg_ll.h>
#include <soc/mipi_dsi_periph.h>

// GNU --wrap redirects the driver's external esp_intr_alloc reference here.
// Keep the real reference strong so the SDK allocator is extracted from its
// static archive. In an ordinary Arduino build without the optional linker
// flag, --gc-sections drops the unused wrapper and observation stays inactive.
extern "C" esp_err_t __real_esp_intr_alloc(int, int, intr_handler_t, void*, intr_handle_t*);

namespace {
static_assert(std::atomic<uint32_t>::is_always_lock_free, "ISR counters must be lock-free");
// Static internal DRAM only: no allocation, logging, I2C or timer task in ISR.
DRAM_ATTR intr_handler_t originalHandler = nullptr;
DRAM_ATTR void* originalArgument = nullptr;
DRAM_ATTR std::atomic<uint32_t> active{0}, installing{0}, sequence{0};
DRAM_ATTR std::atomic<uint32_t> allocatedFlags{0};
DRAM_ATTR std::atomic<uint32_t> interrupts{0}, underruns{0}, lastStatus{0};
DRAM_ATTR std::atomic<uint32_t> lastUsLow{0}, lastUsHigh{0};

void IRAM_ATTR observeBridge(void*) {
  // IDF 5.5.4 reads and clears this status at the beginning of its handler.
  // Reading int_st is non-destructive. Never acknowledge, mask or reset it:
  // the original driver remains the sole owner of the hardware interrupt.
  const uint32_t status = mipi_dsi_brg_ll_get_interrupt_status(MIPI_DSI_LL_GET_BRG(0));
  interrupts.fetch_add(1, std::memory_order_relaxed);
  lastStatus.store(status, std::memory_order_relaxed);
  // This unconditional tail call preserves acknowledgement, SDK diagnostics,
  // VSYNC callbacks and ISR scheduling for every status, including zero.
  originalHandler(originalArgument);
}
}

// Strong dependency of the isolated native DSI driver: a build missing this
// hook must fail to link. The hot path records only internal lock-free state.
extern "C" void IRAM_ATTR tab5_display_underrun_isr(void) {
  sequence.fetch_add(1);
  const uint64_t now = static_cast<uint64_t>(esp_timer_get_time());
  lastUsLow.store(static_cast<uint32_t>(now));
  lastUsHigh.store(static_cast<uint32_t>(now >> 32));
  underruns.fetch_add(1);
  sequence.fetch_add(1);
}

extern "C" esp_err_t __wrap_esp_intr_alloc(int source, int flags,
    intr_handler_t handler, void* argument, intr_handle_t* result) {
#if ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 4)
  uint32_t expected = 0;
  if (source == soc_mipi_dsi_signals[0].brg_irq_id && handler
      && !active.load() && installing.compare_exchange_strong(expected, 1)) {
    originalHandler = handler;
    originalArgument = argument;
    const esp_err_t error = __real_esp_intr_alloc(source, flags, observeBridge, argument, result);
    if (error == ESP_OK) { allocatedFlags.store(static_cast<uint32_t>(flags)); active.store(1); }
    else { originalHandler = nullptr; originalArgument = nullptr; }
    installing.store(0);
    return error;
  }
#endif
  return __real_esp_intr_alloc(source, flags, handler, argument, result);
}

Tab5DisplaySignalsStats tab5DisplaySignalsStats() {
  Tab5DisplaySignalsStats result;
  uint32_t before;
  do {
    before = sequence.load();
    if (before & 1) continue;
    result.interrupts = interrupts.load();
    result.underruns = underruns.load();
    result.lastStatus = lastStatus.load();
    result.lastUnderrunUs = (uint64_t(lastUsHigh.load()) << 32) | lastUsLow.load();
  } while ((before & 1) || before != sequence.load());
  result.active = active.load() != 0;
  result.allocatedFlags = allocatedFlags.load();
  return result;
}
bool tab5DisplayTakeUnderrunEvent(uint32_t nowMs, uint32_t* delta) {
  // Single UI-task consumer. Counter remains cumulative; only logs coalesce.
  static uint32_t reported = 0, lastReportMs = 0;
  static bool reportedOnce = false;
  if (!delta) return false;
  const uint32_t total = underruns.load();
  const uint32_t pending = total - reported;
  if (!pending || (reportedOnce && uint32_t(nowMs - lastReportMs) < 1000)) return false;
  *delta = pending;
  reported = total;
  lastReportMs = nowMs;
  reportedOnce = true;
  return true;
}
#else
Tab5DisplaySignalsStats tab5DisplaySignalsStats() { return {}; }
bool tab5DisplayTakeUnderrunEvent(uint32_t, uint32_t*) { return false; }
#endif
