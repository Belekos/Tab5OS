#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr int ESP_OK = 0;
#define IRAM_ATTR
#define DRAM_ATTR
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,4)
#define CONFIG_IDF_TARGET_ESP32P4 1
int64_t esp_timer_get_time();
