/* The PC produces samples in guest time; I2S consumes them in wall time.
 * Keep I2S at 22050 Hz and interpolate a bounded source FIFO at the measured
 * guest rate. A slow guest therefore sounds slower/lower, without recurring
 * holes caused by playing 56/79 percent of a second at full speed. This does
 * not accelerate the emulated PC or hide its measured speed.
 * Two output blocks cover short worker jitter. A long stall re-primes instead
 * of accumulating an unbounded catch-up burst. Guest DMA/timers run at mute.
 */
#pragma once
#include "pc.h"
#include <stddef.h>
#include <stdint.h>

#define DOS_AUDIO_CAPACITY 4096u
#define DOS_AUDIO_TARGET 1536u
#define DOS_AUDIO_PRIME 2048u
#define DOS_AUDIO_BLOCK 1024u
#define DOS_AUDIO_MIN_RATE 16384u /* 25 percent: bounded interpolation */
#define DOS_AUDIO_MAX_RATE 81920u /* 125 percent: bounded catch-up */
#define DOS_AUDIO_STALL_MS 150u

static int dos_audio_level = 80, dos_audio_saved = 80, dos_audio_error;
static int dos_audio_tail;
static int16_t dos_audio_buffer[512 * 2], dos_audio_input[512 * 2];
static int16_t dos_audio_fifo[DOS_AUDIO_CAPACITY * 2];
static uint32_t dos_audio_head, dos_audio_count, dos_audio_phase;
static uint32_t dos_audio_rate, dos_audio_clock_at, dos_audio_clock_frames;
static uint32_t dos_audio_last_at, dos_audio_remainder, dos_audio_partial;
static unsigned dos_audio_clock_valid, dos_audio_playing, dos_audio_rebuffers;

static int dos_audio_volume(void) { return dos_audio_level; }
static int dos_audio_failed(void) { return dos_audio_error; }
static void dos_audio_reset_stream(void) {
    dos_audio_head = dos_audio_count = dos_audio_phase = 0;
    dos_audio_playing = dos_audio_remainder = 0;
}
static void dos_audio_start(void) {
    dos_audio_error = dos_audio_tail = 0;
    dos_audio_clock_valid = dos_audio_clock_frames = dos_audio_rebuffers = 0;
    dos_audio_partial = 0;
    dos_audio_rate = 65536;
    dos_audio_reset_stream();
    pc_audio_enable(true);
}
static void dos_audio_clear_muted(void) {
    if (!dos_audio_level)
        for (uint32_t i = 0; i < DOS_AUDIO_CAPACITY * 2; ++i) dos_audio_fifo[i] = 0;
}
static void dos_audio_change(int delta) {
    int next = dos_audio_level + delta;
    dos_audio_level = next < 0 ? 0 : next > 100 ? 100 : next;
    if (dos_audio_level) dos_audio_saved = dos_audio_level;
    dos_audio_clear_muted();
    dos_audio_error = 0;
}
static void dos_audio_mute(void) {
    if (dos_audio_level) { dos_audio_saved = dos_audio_level; dos_audio_level = 0; }
    else dos_audio_level = dos_audio_saved;
    dos_audio_clear_muted();
    dos_audio_error = 0;
}
static int dos_audio_submit(unsigned frames) {
    if (!frames) return 0;
    if (!tab5_api->audio_pcm ||
        tab5_api->audio_pcm(dos_audio_buffer, frames, PC_AUDIO_HZ) < 0) {
        dos_audio_error = 1;
        dos_audio_tail = 0;
        dos_audio_partial = 0; /* the failed native sink is released */
        dos_audio_reset_stream();
        return -1;
    }
    dos_audio_partial = (dos_audio_partial + frames) % DOS_AUDIO_BLOCK;
    return 0;
}
/* Pad only a final partial OS block, so a short sound cannot remain deferred. */
static int dos_audio_finish(void) {
    unsigned left = dos_audio_partial ? DOS_AUDIO_BLOCK - dos_audio_partial : 0;
    for (unsigned i = 0; i < 512 * 2; ++i) dos_audio_buffer[i] = 0;
    while (left) {
        unsigned n = left > 512 ? 512 : left;
        if (dos_audio_submit(n) < 0) return -1;
        left -= n;
    }
    dos_audio_reset_stream();
    return 0;
}
/* -1 exactly when output first fails; all pending guest sound still advances. */
static int dos_audio_pump(uint32_t now_ms) {
    uint32_t elapsed = dos_audio_clock_valid ? now_ms - dos_audio_last_at : 0;
    int discontinuity = dos_audio_clock_valid && elapsed > DOS_AUDIO_STALL_MS;
    if (!dos_audio_clock_valid || discontinuity) {
        dos_audio_clock_at = now_ms;
        dos_audio_clock_frames = 0;
        if (discontinuity) {
            ++dos_audio_rebuffers;
            dos_audio_reset_stream();
        }
    }
    dos_audio_last_at = now_ms;
    uint32_t produced = 0;
    while (pc_audio_pending() > 0) {
        int frames = pc_audio_render(dos_audio_input, 512), audible = 0;
        if (frames <= 0) break;
        produced += (uint32_t)frames;
        if (dos_audio_error) continue;
        for (int i = 0; i < frames * 2; ++i) {
            if (!dos_audio_level) dos_audio_input[i] = 0;
            audible |= dos_audio_input[i] != 0;
        }
        if (audible) dos_audio_tail = PC_AUDIO_HZ / 10;
        else if (!dos_audio_tail) continue;
        else dos_audio_tail = frames < dos_audio_tail ? dos_audio_tail - frames : 0;
        for (int i = 0; i < frames; ++i) {
            if (dos_audio_count == DOS_AUDIO_CAPACITY) {
                dos_audio_head = (dos_audio_head + 1) % DOS_AUDIO_CAPACITY;
                --dos_audio_count; /* bounded latency after a guest catch-up */
            }
            uint32_t at = (dos_audio_head + dos_audio_count++) % DOS_AUDIO_CAPACITY;
            dos_audio_fifo[at * 2] = dos_audio_input[i * 2];
            dos_audio_fifo[at * 2 + 1] = dos_audio_input[i * 2 + 1];
        }
    }
    if (dos_audio_clock_valid && !discontinuity) dos_audio_clock_frames += produced;
    dos_audio_clock_valid = 1;
    uint32_t observed_ms = now_ms - dos_audio_clock_at;
    if (observed_ms >= 100) {
        uint64_t rate = ((uint64_t)dos_audio_clock_frames * 1000u << 16) /
                        ((uint64_t)observed_ms * PC_AUDIO_HZ);
        uint32_t measured = rate < DOS_AUDIO_MIN_RATE ? DOS_AUDIO_MIN_RATE :
                            rate > DOS_AUDIO_MAX_RATE ? DOS_AUDIO_MAX_RATE : (uint32_t)rate;
        dos_audio_rate = (dos_audio_rate + measured) / 2;
        dos_audio_clock_at = now_ms;
        dos_audio_clock_frames = 0;
    }
    if (dos_audio_error) return 0;
    uint32_t due;
    if (!dos_audio_playing) {
        uint32_t threshold = DOS_AUDIO_TARGET + ((DOS_AUDIO_PRIME * dos_audio_rate) >> 16);
        if (dos_audio_count < threshold && (dos_audio_tail || dos_audio_count < 2)) return 0;
        dos_audio_playing = 1;
        dos_audio_remainder = 0;
        due = DOS_AUDIO_PRIME;
    } else {
        uint32_t total = elapsed * PC_AUDIO_HZ + dos_audio_remainder;
        due = total / 1000;
        dos_audio_remainder = total % 1000;
    }
    /* Correct reservoir drift gently; use the expected post-output occupancy,
     * so ordinary per-frame batching does not modulate the playback pitch. */
    int32_t expected = (int32_t)DOS_AUDIO_TARGET + (int32_t)((due * dos_audio_rate) >> 16);
    int32_t correction = ((int32_t)dos_audio_count - expected) * 65536 / (PC_AUDIO_HZ / 2);
    int32_t limit = (int32_t)dos_audio_rate / 4;
    if (correction < -limit) correction = -limit;
    if (correction > limit) correction = limit;
    uint32_t step = (uint32_t)((int32_t)dos_audio_rate + correction);
    if (step < DOS_AUDIO_MIN_RATE) step = DOS_AUDIO_MIN_RATE;
    if (step > DOS_AUDIO_MAX_RATE) step = DOS_AUDIO_MAX_RATE;
    unsigned used = 0;
    while (due--) {
        uint32_t advance = (dos_audio_phase + step) >> 16;
        if (dos_audio_count < 2 || advance >= dos_audio_count) {
            if (used && dos_audio_submit(used) < 0) return -1;
            if (dos_audio_tail) ++dos_audio_rebuffers;
            return dos_audio_finish();
        }
        uint32_t a = dos_audio_head, b = (a + 1) % DOS_AUDIO_CAPACITY;
        for (unsigned channel = 0; channel < 2; ++channel) {
            int32_t first = dos_audio_fifo[a * 2 + channel];
            int32_t difference = (int32_t)dos_audio_fifo[b * 2 + channel] - first;
            /* 15-bit fraction keeps the signed product within int32_t. */
            int32_t sample = first + ((difference * (int32_t)(dos_audio_phase >> 1)) >> 15);
            dos_audio_buffer[used * 2 + channel] = (int16_t)(sample * dos_audio_level / 100);
        }
        dos_audio_phase = (dos_audio_phase + step) & 65535u;
        dos_audio_head = (dos_audio_head + advance) % DOS_AUDIO_CAPACITY;
        dos_audio_count -= advance;
        if (++used == 512) {
            if (dos_audio_submit(used) < 0) return -1;
            used = 0;
        }
    }
    return dos_audio_submit(used);
}
