/* Tab5 ESP32-P4 adaptation of Espressif elf_loader 1.3.3 (Apache-2.0).
 * P4 instruction and data use the same PSRAM mapping; never mirror ESP32-S3
 * addresses or globally suspend interrupts/caches for an app launch. */
#include "private/elf_platform.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include <stdint.h>
#include "esp_elf.h"
#include <string.h>
#if !CONFIG_IDF_TARGET_ESP32P4
#error "Tab5 native apps require ESP32-P4"
#endif
static size_t code_alignment(void) {
    size_t alignment=128;
    if(esp_cache_get_alignment(MALLOC_CAP_SPIRAM,&alignment)!=ESP_OK || alignment<128)alignment=128;
    return alignment;
}
void* esp_elf_malloc(uint32_t bytes,bool executable) {
    (void)executable;
    const size_t align=code_alignment();
    if(bytes>SIZE_MAX-(align-1))return NULL;
    return heap_caps_aligned_alloc(align,(bytes+align-1)&~(align-1),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}
void esp_elf_free(void* p) {heap_caps_free(p);}
int tab5_elf_sync(esp_elf_t* elf,size_t bytes) {
    const size_t align=code_alignment();
    if(!elf||!elf->psegment||!bytes||bytes>SIZE_MAX-(align-1))return ESP_ERR_INVALID_ARG;
    size_t rounded=(bytes+align-1)&~(align-1);
    int result=esp_cache_msync(elf->psegment,rounded,ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
    if(result)return result;
    result=esp_cache_msync(elf->psegment,rounded,ESP_CACHE_MSYNC_FLAG_DIR_M2C|ESP_CACHE_MSYNC_FLAG_TYPE_INST);
    __asm__ volatile("fence.i" ::: "memory");
    return result;
}
/* There is deliberately no default libc/FreeRTOS symbol list. */
uintptr_t elf_find_sym_default(const char* name) {return esp_elf_find_symbol(name);}
