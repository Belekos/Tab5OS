#pragma once
#include <stdint.h>

struct Tab5DisplayBandwidthStats {
  uint32_t initialReadPriorities = 0;
  uint32_t initialWritePriorities = 0;
  uint32_t currentReadPriorities = 0;
  uint32_t currentWritePriorities = 0;
  uint8_t initialDisplayRead = 0;
  uint8_t targetDisplayRead = 0;
  uint8_t currentDisplayRead = 0;
  bool supported = false;
  bool verified = false;
  bool flashEncrypted = false;
};

// Called after the known Tab5 DSI framebuffer has been validated. The display
// shares the PSRAM bus with cache and JPEG DMA; only its read arbitration changes.
void tab5DisplayBandwidthInit();
Tab5DisplayBandwidthStats tab5DisplayBandwidthStats();

