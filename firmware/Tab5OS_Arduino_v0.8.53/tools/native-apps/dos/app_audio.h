/* Submit guest PCM at its original rate and pitch. Never manufacture audio
 * from elapsed host time: a blocking speaker callback must not cause the next
 * frame to submit more sound and delay emulation again. Slow guests may leave
 * gaps; maintaining full-speed pitch takes precedence over stretching sound.
 * The guest's sound timers and DMA still advance when output is muted/failed.
 */
#pragma once
#include "pc.h"
#include <stddef.h>
#include <stdint.h>

#define DOS_AUDIO_BLOCK 1024u
#define DOS_AUDIO_BUDGET 1024u
static int dos_audio_level = 80, dos_audio_saved = 80, dos_audio_error;
static int dos_audio_tail;
static unsigned dos_audio_partial;
static int16_t dos_audio_buffer[512 * 2];
static int dos_audio_volume(void) { return dos_audio_level; }
static int dos_audio_failed(void) { return dos_audio_error; }
static void dos_audio_start(void) {
    dos_audio_error = dos_audio_tail = 0;
    dos_audio_partial = 0;
    pc_audio_enable(true);
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
    if (!tab5_api->audio_pcm || tab5_api->audio_pcm(samples, frames, PC_AUDIO_HZ) < 0) {
        dos_audio_error = 1;
        dos_audio_tail = 0;
        dos_audio_partial = 0; /* the failed native sink is released */
        return -1;
    }
    dos_audio_partial = (dos_audio_partial + frames) % DOS_AUDIO_BLOCK;
    return 0;
}
/* Complete only a final partial OS block. This is at most 1023 zero frames,
 * once per sound ending/mute, never a wall-clock catch-up stream. */
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
/* -1 exactly when output first fails; all pending guest samples are drained.
 * Bound a delayed producer's output to recent PCM instead of blocking to
 * replay a backlog. Normal 60 Hz guest frames contain only 367/368 samples. */
static int dos_audio_pump(void) {
    unsigned pending = (unsigned)pc_audio_pending();
    unsigned discard = pending > DOS_AUDIO_BUDGET ? pending - DOS_AUDIO_BUDGET : 0;
    int failed_now = 0;
    while (pc_audio_pending() > 0) {
        int frames = pc_audio_render(dos_audio_buffer, 512), audible = 0;
        if (frames <= 0) break;
        if (dos_audio_error || !dos_audio_level) continue;
        unsigned skip = discard < (unsigned)frames ? discard : (unsigned)frames;
        discard -= skip;
        int16_t *samples = dos_audio_buffer + skip * 2;
        frames -= (int)skip;
        if (!frames) continue;
        for (int i = 0; i < frames * 2; ++i) {
            int32_t sample = (int32_t)samples[i] * dos_audio_level / 100;
            samples[i] = (int16_t)sample;
            audible |= sample != 0;
        }
        if (audible) dos_audio_tail = PC_AUDIO_HZ / 10;
        else if (!dos_audio_tail) continue;
        else dos_audio_tail = frames < dos_audio_tail ? dos_audio_tail - frames : 0;
        if (dos_audio_submit(samples, (unsigned)frames) < 0) failed_now = -1;
    }
    if (!dos_audio_level) dos_audio_tail = 0;
    if (!dos_audio_error && !dos_audio_tail && dos_audio_partial)
        if (dos_audio_finish() < 0) failed_now = -1;
    return failed_now;
}
