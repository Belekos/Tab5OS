#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "multi_heap.h"
#include "tlsf.h"

static unsigned lock_depth;
static uint64_t lock_start, lock_elapsed, visits;
static unsigned lock_count;
static uint64_t clock_ns(void) {
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return (uint64_t)ts.tv_sec * 1000000000U + ts.tv_nsec;
}
void audit_heap_lock(void *lock) {
    assert(lock && !lock_depth);
    lock_depth = 1;
    ++lock_count;
    lock_start = clock_ns();
}
void audit_heap_unlock(void *lock) {
    assert(lock && lock_depth == 1);
    lock_elapsed += clock_ns() - lock_start;
    lock_depth = 0;
}

/* GNU ld wrapping observes every visit without changing TLSF's traversal,
 * callback result, or allocator. It also checks the lock is held throughout. */
struct Walker { tlsf_walker caller; void *context; };
static bool count_visit(void *ptr, size_t size, int used, void *context) {
    struct Walker *w = context;
    assert(lock_depth == 1);
    ++visits;
    return w->caller(ptr, size, used, w->context);
}
void __real_tlsf_walk_pool(pool_t pool, tlsf_walker walker, void *user);
void __wrap_tlsf_walk_pool(pool_t pool, tlsf_walker walker, void *user) {
    struct Walker w = { walker, user };
    __real_tlsf_walk_pool(pool, count_visit, &w);
}

static int cmp_u64(const void *a, const void *b) {
    const uint64_t aa = *(const uint64_t *)a, bb = *(const uint64_t *)b;
    return (aa > bb) - (aa < bb);
}
static void experiment(size_t count, bool comma) {
    /* This host-only arena size also keeps the upstream 32-bit-oriented TLSF
     * metadata layout aligned in LP64. It is not the Tab5 PSRAM capacity. */
    const size_t arena_bytes = 64U * 1024 * 1024;
    void *arena = aligned_alloc(128, arena_bytes);
    void **pointers = calloc(count, sizeof(*pointers));
    assert(arena && pointers);
    multi_heap_handle_t heap = multi_heap_register(arena, arena_bytes);
    assert(heap);
    int guard = 0;
    multi_heap_set_lock(heap, &guard);
    for (size_t i = 0; i < count; ++i) {
        pointers[i] = multi_heap_malloc(heap, 32 + (i % 16) * 16);
        assert(pointers[i]);
    }
    for (size_t i = 0; i < count; i += 2) multi_heap_free(heap, pointers[i]);
    uint64_t durations[31];
    multi_heap_info_t info = {0};
    for (size_t i = 0; i < 31; ++i) {
        lock_elapsed = visits = lock_count = 0;
        multi_heap_get_info(heap, &info);
        assert(visits == info.total_blocks && visits == count + 1);
        assert(lock_count == 1 && !lock_depth);
        durations[i] = lock_elapsed;
    }
    qsort(durations, 31, sizeof(*durations), cmp_u64);
    lock_elapsed = visits = lock_count = 0;
    volatile size_t free_bytes = 0;
    uint64_t started = clock_ns();
    for (unsigned i = 0; i < 10000; ++i) free_bytes = multi_heap_free_size(heap);
    uint64_t counter_elapsed = clock_ns() - started;
    assert(free_bytes == info.total_free_bytes && visits == 0 && lock_count == 0);
    printf("%s{\"allocations\":%zu,\"physical_blocks_walked_with_lock\":%zu,"
           "\"host_lock_ns_median\":%" PRIu64 ",\"host_lock_ns_max\":%" PRIu64 ","
           "\"counter_reads\":10000,\"counter_read_total_ns\":%" PRIu64 ","
           "\"counter_walks\":0,\"free_bytes\":%zu}",
           comma ? ",\n" : "", count, info.total_blocks, durations[15],
           durations[30], counter_elapsed, info.total_free_bytes);
    for (size_t i = 1; i < count; i += 2) multi_heap_free(heap, pointers[i]);
    free(pointers);
    free(arena);
}
int main(void) {
    printf("{\"passed\":true,\"platform\":\"host; not ESP32-P4 timing\",\"runs\":[\n");
    experiment(100, false);
    experiment(1000, true);
    experiment(10000, true);
    experiment(100000, true);
    puts("\n]}");
    return 0;
}
