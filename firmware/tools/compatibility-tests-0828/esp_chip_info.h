#pragma once
#include <stdint.h>
#define CONFIG_ESP_REV_MIN_FULL 1
#define CONFIG_ESP_REV_MAX_FULL 199
struct esp_chip_info_t { uint16_t revision; };
inline void esp_chip_info(esp_chip_info_t* chip) { chip->revision = 103; }
