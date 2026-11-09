/* Guest frames share a monotonic deadline. A costly displayed frame may use
 * the spare time in following frames instead of slowing the guest forever.
 * The caller still invokes sleep_ms(0) when late, preserving SDK cooperation. */
#pragma once
#include <stdint.h>

struct dos_frame_pacer {
    uint32_t deadline;
    unsigned hz, fraction;
};

static uint32_t dos_frame_delay(struct dos_frame_pacer *pacer,
                               uint32_t started, uint32_t now, int video_hz)
{
    const unsigned hz = video_hz > 0 && video_hz <= 1000 ? (unsigned)video_hz : 60u;
    if (pacer->hz != hz) {
        pacer->deadline = started;
        pacer->hz = hz;
        pacer->fraction = 0;
    }
    uint32_t period = 1000u / hz;
    pacer->fraction += 1000u % hz;
    if (pacer->fraction >= hz) {
        pacer->fraction -= hz;
        ++period;
    }
    pacer->deadline += period;
    int32_t remaining = (int32_t)(pacer->deadline - now);
    /* Keep at most 100 ms of catch-up debt. Loading/saving or a sustained
     * overload must not trigger an unbounded burst of catch-up frames.
     * Unsigned elapsed time and signed deadline distance survive millis wrap. */
    if (now - started >= 100u || remaining < -100) {
        pacer->deadline = now;
        pacer->fraction = 0;
        return 0;
    }
    if (now - started >= period) return 0;
    return remaining > 0 ? (uint32_t)remaining : 0;
}
