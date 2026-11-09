/* Native DOS audio output. The guest still advances its DMA/timers at mute.
 * Only nonzero buffers claim the OS speaker. After a sound, a bounded silence
 * tail drains the OS's partial 1024-frame block before idle output stops.
 * Output failure is latched until the user adjusts volume. */
#pragma once
#include "pc.h"
#include <stddef.h>
#include <stdint.h>
static int dos_audio_level = 80, dos_audio_saved = 80, dos_audio_error;
static int dos_audio_tail;
static int16_t dos_audio_buffer[512 * 2];
static int dos_audio_volume(void) { return dos_audio_level; }
static int dos_audio_failed(void) { return dos_audio_error; }
static void dos_audio_start(void) {
    dos_audio_error = 0;
    dos_audio_tail = 0;
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
/* -1 exactly when output first fails; the UI can display one useful notice. */
static int dos_audio_pump(void) {
    int failed_now = 0;
    while (pc_audio_pending() > 0) {
        int frames = pc_audio_render(dos_audio_buffer, 512), audible = 0;
        if (frames <= 0) break;
        if (dos_audio_error) continue;
        for (int i = 0; i < frames * 2; i++) {
            int32_t sample = (int32_t)dos_audio_buffer[i] * dos_audio_level / 100;
            dos_audio_buffer[i] = (int16_t)sample;
            audible |= sample != 0;
        }
        if (audible) dos_audio_tail = PC_AUDIO_HZ / 10; /* 100 ms > one OS block */
        else if (!dos_audio_tail) continue;
        else dos_audio_tail = frames < dos_audio_tail ? dos_audio_tail - frames : 0;
        if (!tab5_api->audio_pcm || tab5_api->audio_pcm(dos_audio_buffer, (size_t)frames, PC_AUDIO_HZ) < 0) {
            dos_audio_error = 1;
            dos_audio_tail = 0;
            failed_now = -1;
        }
    }
    return failed_now;
}
