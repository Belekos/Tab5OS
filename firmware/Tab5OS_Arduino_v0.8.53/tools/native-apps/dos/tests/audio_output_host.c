/* Exercise the actual frontend against a wall-clock sink: M5Speaker has two
 * queued 1024-frame blocks, and the native driver accumulates a partial block.
 * A full queue blocks its producer while I2S continues to consume samples. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int output(const int16_t*, size_t, uint32_t);
static const struct { int (*audio_pcm)(const int16_t*, size_t, uint32_t); }
    api = {output}, *tab5_api = &api;
#include "../app_audio.h"

static uint64_t wall, missing, played, nonzero, submitted, blocked, first_at;
static unsigned queued, front, offset, partial, calls, gaps, started, starved;
static unsigned remaining, enabled, reject, silence, invalid, checks, source_pattern, source_sequence;
static uint64_t changes, positive, negative;
static int played_left, channel_mismatch;
static int16_t blocks[2][2048], building[2048];
static int maximum, minimum, last_left, last_right;
static int source_left, source_right;
static uint32_t clock_origin;

#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr,"FAIL line %d: %s (wall=%llu missing=%llu fifo=%u rate=%u rebuffer=%u)\n", \
        __LINE__,#x,(unsigned long long)wall,(unsigned long long)missing, \
        dos_audio_count,dos_audio_rate,dos_audio_rebuffers); return 1; } } while (0)

void pc_audio_enable(bool on) { enabled = on; }
int pc_audio_pending(void) { return (int)remaining; }
int pc_audio_render(int16_t *dst, int frames) {
    unsigned count = remaining < (unsigned)frames ? remaining : (unsigned)frames;
    remaining -= count;
    for (unsigned i = 0; i < count; ++i) {
        if (source_pattern) { source_left = (int)(source_sequence++ % 1000) * 32 - 16000; source_right = -source_left; }
        dst[2*i] = silence ? 0 : (int16_t)source_left;
        dst[2*i+1] = silence ? 0 : (int16_t)source_right;
    }
    return (int)count;
}

static uint32_t now(void) { return clock_origin + (uint32_t)(wall * 1000 / PC_AUDIO_HZ); }
static void advance(unsigned frames) {
    wall += frames;
    while (frames && queued) {
        unsigned n = frames < 1024-offset ? frames : 1024-offset;
        for (unsigned i = 0; i < n; ++i) {
            int left = blocks[front][2*(offset+i)], right = blocks[front][2*(offset+i)+1];
            nonzero += left != 0 || right != 0;
            changes += left != played_left; played_left = left;
            positive += left > 0; negative += left < 0;
            if (source_pattern && (left+right < -2 || left+right > 2)) channel_mismatch = 1;
        }
        played += n; offset += n; frames -= n;
        if (offset == 1024) { offset = 0; front ^= 1; --queued; }
    }
    if (frames && started) {
        missing += frames;
        if (!starved) { ++gaps; starved = 1; }
    }
}
static int output(const int16_t *pcm, size_t frames, uint32_t rate) {
    ++calls;
    if (!frames || frames > 512 || rate != 22050) { invalid = 1; return -1; }
    if (reject) {
        /* appAudio releases its terminally failed sink before a later retry. */
        partial = queued = front = offset = 0;
        return -1;
    }
    for (size_t i = 0; i < frames; ++i) {
        int left = pcm[2*i], right = pcm[2*i+1];
        if (left > maximum) maximum = left;
        if (right < minimum) minimum = right;
        last_left = left; last_right = right;
        building[2*partial] = (int16_t)left;
        building[2*partial+1] = (int16_t)right;
        ++partial; ++submitted;
        if (partial == 1024) {
            if (queued == 2) { unsigned wait = 1024-offset; blocked += wait; advance(wait); }
            memcpy(blocks[(front+queued)%2], building, sizeof(building));
            ++queued; partial = 0;
            if (!started) first_at = wall;
            started = 1; starved = 0;
        }
    }
    return 0;
}
static void reset(uint32_t origin) {
    wall = missing = played = nonzero = submitted = blocked = first_at = 0;
    queued = front = offset = partial = calls = gaps = started = starved = 0;
    remaining = enabled = reject = silence = invalid = 0;
    maximum = minimum = last_left = last_right = 0;
    source_left = 10000; source_right = -10000;
    source_pattern = source_sequence = 0;
    changes = positive = negative = 0; played_left = channel_mismatch = 0;
    clock_origin = origin;
    dos_audio_level = dos_audio_saved = 80;
    dos_audio_start();
}
static int pump(unsigned frames) { remaining += frames; return dos_audio_pump(now()); }
static int tick(unsigned speed, unsigned weight, unsigned index) {
    /* One 60 Hz guest frame. Weights are tenths of its usual wall duration.
     * Blocking in output() adds real producer delay rather than free CPU time. */
    unsigned frames = (index & 1) ? 368 : 367;
    advance((frames * 100 * weight + speed * 5) / (speed * 10));
    return pump(frames);
}

static int steady(unsigned speed, unsigned jitter, uint32_t origin) {
    static const unsigned moderate[] = {5,10,15,10};
    static const unsigned severe[] = {5,5,20,10};
    reset(origin);
    unsigned max_latency_ms = 0;
    for (unsigned i = 0; i < 2400; ++i) {
        unsigned weight = jitter == 1 ? moderate[i%4] : jitter == 2 ? severe[i%4] : 10;
        CHECK(!tick(speed, weight, i));
        CHECK(!remaining && dos_audio_count <= DOS_AUDIO_CAPACITY && !invalid);
        if (i > 120) {
            unsigned source_ms = (unsigned)((uint64_t)dos_audio_count * 65536 * 1000 / (dos_audio_rate * (uint64_t)PC_AUDIO_HZ));
            unsigned sink_ms = (queued*1024-offset+partial)*1000/PC_AUDIO_HZ;
            if (source_ms+sink_ms > max_latency_ms) max_latency_ms = source_ms+sink_ms;
        }
    }
    printf("TIMELINE speed=%u jitter=%u origin=%u missing=%llu gaps=%u rebuffers=%u rate=%u fifo=%u blocked=%llu startup_ms=%llu max_latency_ms=%u\n",
        speed,jitter,origin,(unsigned long long)missing,gaps,dos_audio_rebuffers,dos_audio_rate,
        dos_audio_count,(unsigned long long)blocked,(unsigned long long)(first_at*1000/PC_AUDIO_HZ),max_latency_ms);
    CHECK(started && nonzero > PC_AUDIO_HZ * 20u && !dos_audio_failed());
    CHECK(first_at < PC_AUDIO_HZ / 2);
    CHECK(max_latency_ms < 400);
    CHECK(dos_audio_rate > speed * 65536 / 100 * 8 / 10);
    CHECK(dos_audio_rate < speed * 65536 / 100 * 12 / 10);
    if (jitter != 2) { CHECK(!missing); CHECK(!dos_audio_rebuffers); }
    else { CHECK(missing < wall / 100); CHECK(dos_audio_rebuffers < 4); }
    CHECK(maximum == 8000 && minimum == -8000);
    return 0;
}

static int transitions(void) {
    static const unsigned speeds[] = {100,56,79,100};
    reset(0);
    for (unsigned section = 0; section < 4; ++section) {
        for (unsigned i = 0; i < 600; ++i) CHECK(!tick(speeds[section],10,i));
        uint64_t before = missing;
        unsigned rebuffer_before = dos_audio_rebuffers;
        for (unsigned i = 0; i < 120; ++i) CHECK(!tick(speeds[section],10,i));
        CHECK(missing == before && dos_audio_rebuffers == rebuffer_before);
    }
    printf("TRANSITIONS missing=%llu gaps=%u rebuffers=%u\n",(unsigned long long)missing,gaps,dos_audio_rebuffers);
    CHECK(missing < PC_AUDIO_HZ / 2 && dos_audio_rebuffers <= 3);
    return 0;
}

static int stall(void) {
    reset(0);
    for (unsigned i = 0; i < 300; ++i) CHECK(!tick(56,10,i));
    unsigned old_rebuffers = dos_audio_rebuffers;
    advance(PC_AUDIO_HZ * 3 / 4);
    CHECK(!pump(368));
    CHECK(dos_audio_rebuffers == old_rebuffers+1 && !dos_audio_playing);
    uint64_t stalled_at = wall;
    for (unsigned i = 0; i < 120 && !dos_audio_playing; ++i) CHECK(!tick(56,10,i));
    CHECK(dos_audio_playing && wall - stalled_at < PC_AUDIO_HZ / 2);
    unsigned recovery_ms = (unsigned)((wall-stalled_at)*1000/PC_AUDIO_HZ);
    uint64_t before = missing;
    for (unsigned i = 0; i < 300; ++i) CHECK(!tick(56,10,i));
    CHECK(missing == before && dos_audio_rebuffers == old_rebuffers+1);
    printf("STALL recovery_ms=%u missing=%llu rebuffers=%u\n",recovery_ms,(unsigned long long)missing,dos_audio_rebuffers);
    return 0;
}

static int controls(void) {
    reset(0); CHECK(enabled && dos_audio_volume() == 80);
    silence = 1; CHECK(!pump(4000) && !remaining && !calls);
    silence = 0; CHECK(!pump(4000) && calls && maximum == 8000 && minimum == -8000);
    dos_audio_change(100); CHECK(dos_audio_volume() == 100);
    for (unsigned i = 0; i < 60; ++i) CHECK(!tick(100,10,i));
    CHECK(last_left == 10000 && last_right == -10000);
    dos_audio_change(-50); dos_audio_mute(); CHECK(!dos_audio_volume());
    for (unsigned i = 0; i < 120; ++i) CHECK(!tick(100,10,i));
    CHECK(!last_left && !last_right && !dos_audio_count && !partial);
    unsigned before_calls = calls;
    for (unsigned i = 0; i < 30; ++i) CHECK(!tick(100,10,i));
    CHECK(calls == before_calls);
    dos_audio_mute(); CHECK(dos_audio_volume() == 50);
    for (unsigned i = 0; i < 120; ++i) CHECK(!tick(100,10,i));
    CHECK(last_left == 5000 && last_right == -5000);
    dos_audio_change(-100); CHECK(!dos_audio_volume()); dos_audio_change(10); CHECK(dos_audio_volume() == 10);
    for (unsigned i = 0; i < 120; ++i) CHECK(!tick(100,10,i));
    CHECK(last_left == 1000 && last_right == -1000);
    reject = 1;
    int result = 0;
    for (unsigned i = 0; i < 60 && !result; ++i) result = tick(100,10,i);
    CHECK(result == -1 && dos_audio_failed() && !remaining && !dos_audio_partial);
    before_calls = calls;
    for (unsigned i = 0; i < 30; ++i) CHECK(!tick(100,10,i));
    CHECK(calls == before_calls && !remaining);
    reject = 0; dos_audio_change(10);
    for (unsigned i = 0; i < 120; ++i) CHECK(!tick(100,10,i));
    CHECK(!dos_audio_failed() && calls > before_calls && last_left == 2000 && last_right == -2000);
    CHECK(dos_audio_partial == partial);
    return 0;
}

static int short_sound(void) {
    reset(0);
    CHECK(!pump(16) && !calls); silence = 1;
    for (unsigned i = 0; i < 180; ++i) CHECK(!tick(56,10,i));
    CHECK(nonzero > 0 && !dos_audio_count && !partial && !dos_audio_partial);
    unsigned before = calls;
    for (unsigned i = 0; i < 30; ++i) CHECK(!tick(56,10,i));
    CHECK(calls == before && !remaining);
    return 0;
}

static int extremes(void) {
    reset(0); source_left = 32767; source_right = -32768;
    CHECK(!pump(4000) && last_left == 26213 && last_right == -26214);
    dos_audio_change(100);
    for (unsigned i = 0; i < 60; ++i) CHECK(!tick(100,10,i));
    CHECK(maximum == 32767 && minimum == -32768);
    /* Interpolate across the whole signed range with a half-sample phase.
     * This exercises the largest signed difference, not just constant PCM. */
    reset(0); dos_audio_level = 100;
    dos_audio_clock_valid = dos_audio_playing = 1;
    dos_audio_clock_at = dos_audio_last_at = 0;
    dos_audio_rate = 32768; dos_audio_phase = 32768;
    dos_audio_count = DOS_AUDIO_TARGET;
    for (unsigned i = 0; i < DOS_AUDIO_TARGET; ++i) {
        dos_audio_fifo[2*i] = (i & 1) ? 32767 : -32768;
        dos_audio_fifo[2*i+1] = (i & 1) ? -32768 : 32767;
    }
    CHECK(!dos_audio_pump(1) && submitted == 22);
    CHECK(building[0] == -1 && building[1] == -1 && !invalid);
    return 0;
}

static int progression(void) {
    reset(0); source_pattern = 1; dos_audio_level = 100;
    for (unsigned i = 0; i < 600; ++i) CHECK(!tick(56,10,i));
    CHECK(!missing && !dos_audio_rebuffers && !channel_mismatch);
    CHECK(changes > played * 9 / 10 && positive > played / 3 && negative > played / 3);
    CHECK(maximum <= 16000 && minimum >= -16000);
    printf("PROGRESSION played=%llu changed=%llu positive=%llu negative=%llu\n",
        (unsigned long long)played,(unsigned long long)changes,
        (unsigned long long)positive,(unsigned long long)negative);
    return 0;
}

int main(void) {
    CHECK(!controls()); CHECK(!short_sound()); CHECK(!extremes()); CHECK(!progression());
    CHECK(!steady(56,0,0)); CHECK(!steady(79,0,0)); CHECK(!steady(100,0,0));
    CHECK(!steady(56,1,0)); CHECK(!steady(79,1,0)); CHECK(!steady(100,1,0));
    CHECK(!steady(56,2,0)); CHECK(!steady(56,1,UINT32_MAX-500));
    CHECK(!transitions()); CHECK(!stall());
    printf("AUDIO OUTPUT PASS: checks=%u volume/mute/restore/tail/failure/retry/56-79-100-percent/jitter/transitions/stall/wrap\n",checks);
    return 0;
}
