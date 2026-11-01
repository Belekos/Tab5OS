#pragma once
#include <stdint.h>
#include <stddef.h>
using esp_err_t=int32_t;
constexpr esp_err_t ESP_OK=0,ESP_ERR_NOT_FOUND=0x105,ESP_ERR_INVALID_SIZE=0x104,ESP_ERR_IMAGE_INVALID=0x2002,ESP_FAIL=-1;
struct esp_partition_t { uint32_t address,size; };
esp_err_t esp_partition_read(const esp_partition_t*,size_t,void*,size_t);
