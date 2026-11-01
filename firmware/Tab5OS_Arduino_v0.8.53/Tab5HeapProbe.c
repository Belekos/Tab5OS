#include "Tab5HeapProbe.h"
#include "sdkconfig.h"
#include "esp_idf_version.h"
#ifndef TAB5_HEAP_PROBE_PINNED
#define TAB5_HEAP_PROBE_PINNED 0
#endif

// Private layout is deliberately confined to this adapter. Other targets,
// SDK versions or allocator options use the public SDK fallback, never a
// guessed layout or a compatibility refusal. The build audit pins libheap.
#if TAB5_HEAP_PROBE_PINNED && defined(CONFIG_IDF_TARGET_ESP32P4) && ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 4) && \
    defined(CONFIG_HEAP_POISONING_LIGHT) && CONFIG_HEAP_POISONING_LIGHT && !CONFIG_HEAP_TASK_TRACKING && \
    !CONFIG_HEAP_TLSF_USE_ROM_IMPL && !CONFIG_HEAP_POISONING_COMPREHENSIVE && \
    UINTPTR_MAX == UINT32_MAX
#include <stdbool.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <soc/soc_memory_layout.h>
#include "esp_heap_caps.h"
#include "multi_heap.h"
#include "Tab5HeapFit.h"

// Exact IDF v5.5.4 heap_private.h layout (task tracking disabled).
typedef struct heap_t_ {
    uint32_t caps[SOC_MEMORY_TYPE_NO_PRIOS];
    intptr_t start, end;
    portMUX_TYPE heap_mux;
    multi_heap_handle_t heap;
    struct { struct heap_t_ *sle_next; } next;
} Tab5RegisteredHeap;
extern struct registered_heap_ll { Tab5RegisteredHeap *slh_first; } registered_heaps;

// Exact IDF v5.5.4 multi_heap.c control prefix. No allocator fields change.
typedef struct multi_heap_info {
    void *lock;
    size_t free_bytes, minimum_free_bytes, pool_size;
    void *heap_data;
} Tab5MultiHeap;
_Static_assert(sizeof(Tab5MultiHeap) == 20, "IDF multi_heap layout changed");
_Static_assert(offsetof(Tab5MultiHeap, heap_data) == 16, "IDF multi_heap layout changed");
_Static_assert(SOC_MEMORY_TYPE_NO_PRIOS == 3, "IDF heap priorities changed");
_Static_assert(sizeof(Tab5RegisteredHeap) == 36, "IDF registered heap layout changed");
_Static_assert(offsetof(Tab5RegisteredHeap, heap_mux) == 20, "IDF heap layout changed");
_Static_assert(offsetof(Tab5RegisteredHeap, heap) == 28, "IDF heap handle offset changed");
_Static_assert(offsetof(Tab5RegisteredHeap, next) == 32, "IDF heap list offset changed");
_Static_assert(sizeof(control_t) == 36, "SDK TLSF layout changed");
extern void multi_heap_internal_lock(multi_heap_handle_t heap);
extern void multi_heap_internal_unlock(multi_heap_handle_t heap);
extern void esp_heap_adjust_alignment_to_hw(size_t *, size_t *, uint32_t *);

int tab5HeapCanAllocate(size_t bytes, uint32_t caps) {
    const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const uint32_t external = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    if (caps != internal && caps != external && caps != (internal | MALLOC_CAP_DMA))
        return TAB5_HEAP_FIT_UNKNOWN;
    if (!bytes) return TAB5_HEAP_FIT_YES;
    size_t alignment = 4;
    esp_heap_adjust_alignment_to_hw(&alignment, &bytes, &caps);
    if (!bytes || bytes > SOC_MAX_CONTIGUOUS_RAM_SIZE || bytes > SIZE_MAX - 12)
        return TAB5_HEAP_FIT_NO;
    // LIGHT poisoning adds an 8-byte head and a 4-byte tail in this ABI.
    const size_t nativeBytes = bytes + 12;
    for (Tab5RegisteredHeap *entry = registered_heaps.slh_first; entry;
         entry = entry->next.sle_next) {
        const uint32_t all = entry->caps[0] | entry->caps[1] | entry->caps[2];
        if (!entry->heap || (all & caps) != caps) continue;
        multi_heap_internal_lock(entry->heap);
        const bool fits = tab5TlsfCanAllocate(((Tab5MultiHeap *)entry->heap)->heap_data,
                                             nativeBytes, alignment);
        multi_heap_internal_unlock(entry->heap);
        if (fits) return TAB5_HEAP_FIT_YES;
    }
    return TAB5_HEAP_FIT_NO;
}
#else
int tab5HeapCanAllocate(size_t bytes, uint32_t caps) {
    (void)bytes; (void)caps;
    return TAB5_HEAP_FIT_UNKNOWN;
}
#endif
