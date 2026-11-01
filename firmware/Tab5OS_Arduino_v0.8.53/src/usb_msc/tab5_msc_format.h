#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "msc_host.h"
#ifdef __cplusplus
extern "C" {
#endif
// Destructive, whole-device format. Caller owns the device, has unmounted its
// VFS and has excluded every filesystem user. Never called by automatic mount.
esp_err_t tab5_msc_format_raw(msc_host_device_handle_t device,bool fat32,
                            bool (*present)(void*),void* context);
#ifdef __cplusplus
}
#endif
