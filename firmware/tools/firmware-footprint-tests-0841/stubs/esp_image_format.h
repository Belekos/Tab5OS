#pragma once
#include "esp_partition.h"
constexpr uint8_t ESP_IMAGE_HEADER_MAGIC=0xe9;
constexpr unsigned ESP_IMAGE_MAX_SEGMENTS=16;
// The production function only reads these two official leading fields.
// Retain the 24-byte on-flash size; real SDK types are used in target compilation.
struct esp_image_header_t { uint8_t magic,segment_count; uint8_t rest[22]; };
struct esp_partition_pos_t { uint32_t offset,size; };
struct esp_image_metadata_t { uint32_t start_addr,image_len; };
esp_err_t esp_image_get_metadata(const esp_partition_pos_t*,esp_image_metadata_t*);
