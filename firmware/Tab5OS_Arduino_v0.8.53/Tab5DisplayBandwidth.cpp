#include "Tab5DisplayBandwidth.h"
#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "Tab5DisplayBandwidthPolicy.h"
#include <M5Unified.h>
#include <esp_flash_encrypt.h>
#include <hal/axi_icm_ll.h>
#include <hal/dw_gdma_ll.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
Tab5DisplayBandwidthStats state;
portMUX_TYPE configLock = portMUX_INITIALIZER_UNLOCKED;

static_assert(DW_GDMA_LL_MASTER_PORT_MEMORY == 1,
              "Recheck the DSI PSRAM read master before changing AXI priorities");

void refresh(Tab5DisplayBandwidthStats& result) {
  result.currentReadPriorities = AXI_ICM.mst_arqos_reg0.val;
  result.currentWritePriorities = AXI_ICM.mst_awqos_reg0.val;
  result.currentDisplayRead = Tab5DisplayBandwidthPolicy::displayRead(result.currentReadPriorities);
  result.verified = result.supported && Tab5DisplayBandwidthPolicy::verify(
      result.initialReadPriorities, result.initialWritePriorities,
      result.currentReadPriorities, result.currentWritePriorities, result.targetDisplayRead);
}
}

void tab5DisplayBandwidthInit() {
  if (M5.getBoard() != m5::board_t::board_M5Tab5 || state.supported) return;
  auto* panel = M5.Display.getPanel();
  if (!panel || !panel->getBus()
      || panel->getBus()->busType() != lgfx::bus_type_t::bus_dsi) return;
  const auto& cfg = panel->config();
  if (cfg.panel_width != 720 || cfg.panel_height != 1280
      || cfg.memory_width != 720 || cfg.memory_height != 1280
      || cfg.offset_x || cfg.offset_y || M5.Display.getColorDepth() != 16) return;

  state.flashEncrypted = esp_flash_encryption_enabled();
  portENTER_CRITICAL(&configLock);
  state.initialReadPriorities = AXI_ICM.mst_arqos_reg0.val;
  state.initialWritePriorities = AXI_ICM.mst_awqos_reg0.val;
  state.initialDisplayRead = Tab5DisplayBandwidthPolicy::displayRead(state.initialReadPriorities);
  state.targetDisplayRead = Tab5DisplayBandwidthPolicy::targetRead(state.initialReadPriorities);

  // IDF's DSI descriptor reads PSRAM through DW master 1 and writes the DSI
  // bridge through master 0. Channel priority alone does not arbitrate against
  // CPU/cache/JPEG masters. Keep writes and every other master's priority intact.
  const uint32_t memoryWrite = (state.initialWritePriorities >> 16) & 15u;
  axi_icm_ll_set_dw_gdma_qos_arbiter_prio(
      DW_GDMA_LL_MASTER_PORT_MEMORY, memoryWrite, state.targetDisplayRead);
  state.supported = true;
  refresh(state);
  portEXIT_CRITICAL(&configLock);
}

Tab5DisplayBandwidthStats tab5DisplayBandwidthStats() {
  auto result = state;
  if (result.supported) refresh(result);
  return result;
}

#else
void tab5DisplayBandwidthInit() {}
Tab5DisplayBandwidthStats tab5DisplayBandwidthStats() { return {}; }
#endif

