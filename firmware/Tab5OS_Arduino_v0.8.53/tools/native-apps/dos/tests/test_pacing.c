#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../pacing.h"

/* Model the SDK contract: even sleep_ms(0) cooperates for one timer tick. */
static uint32_t advance(struct dos_frame_pacer *p, uint32_t now,
                        uint32_t work, int hz, uint32_t *delay)
{
    uint32_t end = now + work;
    *delay = dos_frame_delay(p, now, end, hz);
    assert((int32_t)(p->deadline - end) >= -100);
    return end + (*delay ? *delay : 1u);
}

int main(void)
{
    const int rates[] = {40, 60, 70, 72, 80};
    unsigned checks = 0;
    for (unsigned r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r) {
        struct dos_frame_pacer p = {0};
        uint32_t now = 0, delay;
        for (unsigned i = 1; i <= 1000; ++i) {
            now = advance(&p, now, 5, rates[r], &delay);
            assert(now == (uint32_t)((uint64_t)i * 1000u / (unsigned)rates[r]));
            ++checks;
        }
    }
    /* Periodic video work fits the average budget. The old per-frame wait
     * nevertheless slows the guest because it never recovers that time. */
    struct dos_frame_pacer variable = {0};
    uint32_t now = 0, old_now = 0, delay;
    for (unsigned i = 0; i < 700; ++i) {
        uint32_t work = i % 3 == 0 ? 25u : 5u;
        now = advance(&variable, now, work, 70, &delay);
        old_now += work + (work < 14 ? 14 - work : 1u);
        ++checks;
    }
    assert(now >= 10000 && now <= 10030);
    assert(old_now > now + 1500);

    struct dos_frame_pacer overloaded = {0};
    uint32_t overloaded_now = 0;
    for (unsigned i = 0; i < 1000; ++i) {
        overloaded_now = advance(&overloaded, overloaded_now, 20, 70, &delay);
        assert(delay == 0); /* Already-late frames never request extra sleep. */
        ++checks;
    }
    assert(overloaded_now == 21000);

    struct dos_frame_pacer stalled = {0};
    uint32_t stalled_now = advance(&stalled, 0, 5, 70, &delay);
    stalled_now = advance(&stalled, stalled_now, 250, 70, &delay);
    assert(delay == 0 && stalled.deadline == stalled_now - 1);
    assert(stalled.fraction == 0);
    stalled_now = advance(&stalled, stalled_now, 5, 70, &delay);
    assert(delay > 0 && delay <= 15); /* Resume pacing immediately. */
    checks += 3;

    struct dos_frame_pacer wrapped = {0};
    uint32_t origin = UINT32_MAX - 23u, wrapped_now = origin;
    for (unsigned i = 0; i < 140; ++i) {
        wrapped_now = advance(&wrapped, wrapped_now, 5, 70, &delay);
        ++checks;
    }
    assert(wrapped_now - origin == 2000u);

    struct dos_frame_pacer changed = {0};
    uint32_t changed_now = advance(&changed, 0, 5, 60, &delay);
    changed_now = advance(&changed, changed_now, 25, 60, &delay);
    uint32_t changed_start = changed_now;
    changed_now = advance(&changed, changed_now, 5, 70, &delay);
    assert(changed_now - changed_start == 14u);
    assert(changed.hz == 70 && changed.fraction == 20);
    checks += 3;

    printf("{\"passed\":true,\"checks\":%u,\"variableWorkMs\":%u,"
           "\"previousPacingMs\":%u,\"nominalMs\":10000,"
           "\"maxCatchupDebtMs\":100}\n", checks, now, old_now);
    return 0;
}
