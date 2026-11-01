#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { TAB5_HEAP_FIT_UNKNOWN = -1, TAB5_HEAP_FIT_NO = 0, TAB5_HEAP_FIT_YES = 1 };
// Snapshot only, never a reservation. The eventual allocation remains final.
// Unknown SDK/configurations fall back to the public SDK query at the caller.
int tab5HeapCanAllocate(size_t bytes, uint32_t capabilities);
#ifdef __cplusplus
}
#endif
