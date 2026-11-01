#pragma once
#include <stdint.h>
#if __has_include("tlsf.h")
#include "tlsf.h"
#else
// Arduino exports the SDK heap/include directory, while these pinned private
// headers reside beside it in heap/tlsf. No duplicate allocator is compiled.
#include "../tlsf/include/tlsf.h"
#endif
#if __has_include("tlsf_control_functions.h")
#include "tlsf_control_functions.h"
#else
#include "../tlsf/tlsf_control_functions.h"
#endif

// Non-mutating counterpart of tlsf_malloc/tlsf_memalign_offs in Espressif
// TLSF 2867f6883a12920b1969ff9624c0ab0e4185c2ce. Reuse the SDK's mapping and
// bitmap lookup; do not call block_locate_free, which removes the free block.
// Caller holds the native heap lock and includes allocator wrapper overhead.
static inline bool tab5TlsfCanAllocate(void *tlsf, size_t bytes, size_t alignment) {
    if (!tlsf || !bytes || !alignment || (alignment & (alignment - 1))) return false;
    control_t *control = (control_t *)tlsf;
    if (bytes > SIZE_MAX - (ALIGN_SIZE - 1)) return false;
    size_t adjusted = adjust_request_size(control, bytes, ALIGN_SIZE);
    if (!adjusted) return false;
    if (alignment > ALIGN_SIZE) {
        // Same search size as tlsf_memalign_offs. The rounded data offset
        // cancels in gap_minimum - off_adjust; address trimming happens later.
        if (alignment > SIZE_MAX - sizeof(block_header_t)) return false;
        const size_t extra = alignment + sizeof(block_header_t);
        if (adjusted > SIZE_MAX - extra ||
            adjusted + extra > SIZE_MAX - (alignment - 1)) return false;
        adjusted = adjust_request_size(control, adjusted + extra, alignment);
        if (!adjusted) return false;
    }
    int first = 0, second = 0;
    mapping_search(control, &adjusted, &first, &second);
    if (first < 0 || (unsigned)first >= control->fl_index_count) return false;
    return search_suitable_block(control, &first, &second) != NULL;
}
