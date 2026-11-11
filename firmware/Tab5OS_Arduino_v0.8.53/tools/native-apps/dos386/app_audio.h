/* Tiny386 devices use a host-time clock. Mix and write device registers on
 * the same worker, in bounded chunks; never feed speaker blocking time back
 * into an unbounded PCM catch-up loop. */
#pragma once
#include "machine.h"
#define DOS_AUDIO_BLOCK 1024u
#define DOS_AUDIO_BUDGET 1024u
static int dos_audio_level = 80, dos_audio_saved = 80, dos_audio_error;
static int dos_audio_tail;
static unsigned dos_audio_partial, dos_audio_fraction;
static uint32_t dos_audio_at;
static uint32_t dos_audio_sink_ms;
static int16_t dos_audio_buffer[512 * 2];
static int dos_audio_volume(void) { return dos_audio_level; }
static int dos_audio_failed(void) { return dos_audio_error; }
static void dos_audio_start(void) {
    dos_audio_error = dos_audio_tail = 0;
    dos_audio_partial = dos_audio_fraction = 0;
    dos_audio_at = tab5_api->base.millis();
}
static void dos_audio_change(int delta) {
    int next = dos_audio_level + delta;
    dos_audio_level = next < 0 ? 0 : next > 100 ? 100 : next;
    if (dos_audio_level) dos_audio_saved = dos_audio_level;
    dos_audio_error = 0;
}
static void dos_audio_mute(void) {
    if (dos_audio_level) { dos_audio_saved = dos_audio_level; dos_audio_level = 0; }
    else dos_audio_level = dos_audio_saved;
    dos_audio_error = 0;
}
static int dos_audio_submit(const int16_t *samples, unsigned frames) {
    if (!frames) return 0;
    uint32_t before = tab5_api->base.millis();
    int result = tab5_api->audio_pcm ? tab5_api->audio_pcm(samples, frames, MACHINE_AUDIO_HZ) : -1;
    dos_audio_sink_ms += tab5_api->base.millis() - before;
    if (result < 0) {
        dos_audio_error = 1; dos_audio_tail = 0; dos_audio_partial = 0;
        return -1;
    }
    dos_audio_partial = (dos_audio_partial + frames) % DOS_AUDIO_BLOCK;
    return 0;
}
static int dos_audio_finish(void) {
    unsigned left = dos_audio_partial ? DOS_AUDIO_BLOCK - dos_audio_partial : 0;
    for (unsigned i = 0; i < 512 * 2; ++i) dos_audio_buffer[i] = 0;
    while (left) {
        unsigned n = left > 512 ? 512 : left;
        if (dos_audio_submit(dos_audio_buffer, n) < 0) return -1;
        left -= n;
    }
    return 0;
}
static int dos_audio_pump(void) {
    uint32_t now = tab5_api->base.millis(), elapsed = now - dos_audio_at;
    if (elapsed < 6u) return 0;
    dos_audio_sink_ms = 0;
    if (elapsed > 1000u) elapsed = 1000u;
    unsigned ticks = elapsed * MACHINE_AUDIO_HZ + dos_audio_fraction;
    unsigned frames_left = ticks / 1000u;
    dos_audio_fraction = ticks % 1000u;
    if (frames_left > DOS_AUDIO_BUDGET) frames_left = DOS_AUDIO_BUDGET;
    int failed_now = 0;
    while (frames_left) {
        unsigned frames = frames_left > 512u ? 512u : frames_left;
        machine_audio_render(dos_audio_buffer, frames);
        frames_left -= frames;
        if (dos_audio_error || !dos_audio_level) continue;
        int audible = 0;
        for (unsigned i = 0; i < frames * 2u; ++i) {
            int32_t sample = (int32_t)dos_audio_buffer[i] * dos_audio_level / 100;
            dos_audio_buffer[i] = (int16_t)sample; audible |= sample != 0;
        }
        if (audible) dos_audio_tail = MACHINE_AUDIO_HZ / 10;
        else if (!dos_audio_tail) continue;
        else dos_audio_tail = (int)frames < dos_audio_tail ? dos_audio_tail - (int)frames : 0;
        if (dos_audio_submit(dos_audio_buffer, frames) < 0) failed_now = -1;
    }
    if (!dos_audio_level) dos_audio_tail = 0;
    if (!dos_audio_error && !dos_audio_tail && dos_audio_partial)
        if (dos_audio_finish() < 0) failed_now = -1;
    /* Only native sink blocking is excluded. Mixer computation is real elapsed
     * time and must count, otherwise even a fast guest slowly starves audio. */
    dos_audio_at = now + dos_audio_sink_ms;
    return failed_now;
}
