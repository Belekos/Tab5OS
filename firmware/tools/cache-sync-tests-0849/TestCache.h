#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_cache.h"

#define CONFIG_IDF_TARGET_ESP32P4 1
#define CONFIG_ESP_MM_CACHE_MSYNC_C2M_CHUNKED_OPS 0
#define SOC_CACHE_WRITEBACK_SUPPORTED 1
#define ESP_IDF_VERSION_VAL(a,b,c) (((a)<<16)|((b)<<8)|(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,4)
#define IRAM_ATTR
#define ESP_RETURN_ON_FALSE_ISR(e,r,...) do { if (!(e)) return (r); } while (0)
#define ESP_EARLY_LOGV(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define DEFINE_CRIT_SECTION_LOCK_STATIC(name) static int name
#define esp_os_enter_critical_safe(p) test_enter_critical(p)
#define esp_os_exit_critical_safe(p) test_exit_critical(p)

typedef enum { CACHE_TYPE_DATA, CACHE_TYPE_INSTRUCTION } cache_type_t;
#ifdef __cplusplus
extern "C" {
#endif
bool xPortInIsrContext(void);
bool xPortCanYield(void);
bool esp_ptr_external_ram(const void*);
bool cache_hal_vaddr_to_cache_level_id(uint32_t, uint32_t, uint32_t*, uint32_t*);
uint32_t cache_hal_get_cache_line_size(uint32_t, cache_type_t);
bool cache_hal_writeback_addr(uint32_t, uint32_t);
bool cache_hal_invalidate_addr(uint32_t, uint32_t);
void test_enter_critical(int*);
void test_exit_critical(int*);
esp_err_t tab5_native_cache_msync(void*, size_t, int);
esp_err_t __wrap_esp_cache_msync(void*, size_t, int);
#ifdef __cplusplus
}
#endif
