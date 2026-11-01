#pragma once
// The SDK archive is immutable. Only the reviewed P4/Hosted 5.5.4 allocator
// translation unit is replaced; other SDKs retain their native allocation mode.
#ifndef TAB5_MESH_EXTERNAL_POOLS
#include <sdkconfig.h>
#include <esp_idf_version.h>
#if ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5,5,4) && CONFIG_IDF_TARGET_ESP32P4 && CONFIG_SPIRAM && CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL && CONFIG_ESP_HOSTED_NIMBLE_HCI_VHCI && !CONFIG_BT_NIMBLE_MEM_DEBUG && !CONFIG_BT_LE_USED_MEM_STATISTICS_ENABLED && !CONFIG_BT_LE_MEM_CHECK_ENABLED
#define TAB5_MESH_EXTERNAL_POOLS 1
#else
#define TAB5_MESH_EXTERNAL_POOLS 0
#endif
#endif
