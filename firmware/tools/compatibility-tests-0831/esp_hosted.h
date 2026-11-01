#pragma once
#include "esp_event.h"
#define ESP_HOSTED_VERSION_MAJOR_1 2
#define ESP_HOSTED_VERSION_MINOR_1 12
#define ESP_HOSTED_VERSION_PATCH_1 8
inline const char* ESP_HOSTED_EVENT = "ESP_HOSTED_EVENT";
enum {
  ESP_HOSTED_EVENT_CP_INIT, ESP_HOSTED_EVENT_CP_HEARTBEAT,
  ESP_HOSTED_EVENT_TRANSPORT_FAILURE, ESP_HOSTED_EVENT_TRANSPORT_UP,
  ESP_HOSTED_EVENT_TRANSPORT_DOWN, ESP_HOSTED_EVENT_MEM_MONITOR
};
struct esp_hosted_event_init_t { uint32_t reason; };
struct esp_hosted_coprocessor_fwver_t {
  uint32_t major1, minor1, patch1;
  int32_t revision, prerelease, build;
};
int esp_hosted_get_coprocessor_fwversion(esp_hosted_coprocessor_fwver_t* version);
