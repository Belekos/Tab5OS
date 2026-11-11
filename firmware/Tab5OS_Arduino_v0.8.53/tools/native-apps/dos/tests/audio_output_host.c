/* Real frontend and a wall-clock speaker model: two 1024-frame blocks plus
 * the OS partial block. Assert source identity, bounded work and real blocking;
 * gaps at sub-realtime guest speeds are reported, never hidden by stretching. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int output(const int16_t*, size_t, uint32_t);
static const struct { int (*audio_pcm)(const int16_t*, size_t, uint32_t); }
    api = {output}, *tab5_api = &api;
#include "../app_audio.h"

static uint64_t wall, missing, played, nonzero, submitted, blocked, first_at, rendered;
static unsigned queued, front, offset, partial, calls, gaps, started, starved;
static unsigned remaining, enabled, reject, silence, invalid, checks, source_pattern, source_sequence;
static unsigned expected_sequence, verify_pcm, mismatched_pcm, injected_pause;
static uint64_t changes, positive, negative, verified_frames;
static int played_left;
static int16_t blocks[2][2048], building[2048];
static int maximum, minimum, last_left, last_right;
static int source_left, source_right;
static uint32_t clock_origin;

#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr,"FAIL line %d: %s (wall=%llu submitted=%llu rendered=%llu partial=%u)\n", \
        __LINE__,#x,(unsigned long long)wall,(unsigned long long)submitted, \
        (unsigned long long)rendered,dos_audio_partial); return 1; } } while (0)

void pc_audio_enable(bool on) { enabled = on; }
int pc_audio_pending(void) { return (int)remaining; }
int pc_audio_render(int16_t *dst, int frames) {
    unsigned count = remaining < (unsigned)frames ? remaining : (unsigned)frames;
    remaining -= count; rendered += count;
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
    if (injected_pause) { advance(injected_pause); injected_pause = 0; }
    if (reject) {
        /* appAudio releases its terminally failed sink before a later retry. */
        partial = queued = front = offset = 0;
        return -1;
    }
    for (size_t i = 0; i < frames; ++i) {
        int left = pcm[2*i], right = pcm[2*i+1];
        if (verify_pcm) {
            int expected = (int)(expected_sequence++ % 1000) * 32 - 16000;
            if (left != expected || right != -expected) ++mismatched_pcm;
            ++verified_frames;
        }
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
    wall = missing = played = nonzero = submitted = blocked = first_at = rendered = 0;
    queued = front = offset = partial = calls = gaps = started = starved = 0;
    remaining = enabled = reject = silence = invalid = 0;
    maximum = minimum = last_left = last_right = 0;
    source_left = 10000; source_right = -10000;
    source_pattern = source_sequence = 0;
    expected_sequence = verify_pcm = mismatched_pcm = injected_pause = 0;
    changes = positive = negative = verified_frames = 0; played_left = 0;
    clock_origin = origin;
    dos_audio_level = dos_audio_saved = 80;
    dos_audio_start();
}
static int pump(unsigned frames) { remaining += frames; return dos_audio_pump(); }
static int tick(unsigned speed, unsigned weight, unsigned index) {
    unsigned frames = (index & 1) ? 368 : 367;
    advance((frames * 100 * weight + speed * 5) / (speed * 10));
    return pump(frames);
}

static int steady(unsigned speed, unsigned jitter, uint32_t origin) {
    static const unsigned moderate[] = {5,10,15,10};
    static const unsigned severe[] = {5,5,20,10};
    reset(origin); source_pattern = verify_pcm = 1; dos_audio_level = 100;
    unsigned max_latency_ms = 0;
    for (unsigned i = 0; i < 2400; ++i) {
        unsigned weight = jitter == 1 ? moderate[i%4] : jitter == 2 ? severe[i%4] : 10;
        CHECK(!tick(speed, weight, i));
        CHECK(!remaining && !invalid && !mismatched_pcm);
        CHECK(submitted == rendered && partial == dos_audio_partial);
        unsigned sink_ms = (queued*1024-offset+partial)*1000/PC_AUDIO_HZ;
        if (sink_ms > max_latency_ms) max_latency_ms = sink_ms;
    }
    printf("TIMELINE speed=%u jitter=%u origin=%u missing=%llu gaps=%u generated=%llu submitted=%llu bitexact=%llu blocked=%llu startup_ms=%llu max_latency_ms=%u\n",
        speed,jitter,origin,(unsigned long long)missing,gaps,(unsigned long long)rendered,
        (unsigned long long)submitted,(unsigned long long)verified_frames,(unsigned long long)blocked,
        (unsigned long long)(first_at*1000/PC_AUDIO_HZ),max_latency_ms);
    CHECK(started && nonzero > PC_AUDIO_HZ * 20u && !dos_audio_failed());
    CHECK(first_at < PC_AUDIO_HZ / 2 && max_latency_ms < 140);
    CHECK(verified_frames == rendered && !mismatched_pcm);
    if (speed < 100) CHECK(!blocked);
    if (origin) CHECK(now() < origin); /* simulated millis crossed uint32 wrap */
    return 0;
}

static int controls(void) {
    reset(0); CHECK(enabled && dos_audio_volume() == 80);
    silence = 1; CHECK(!pump(4000) && !remaining && !calls);
    silence = 0; CHECK(!pump(500) && calls && maximum == 8000 && minimum == -8000);
    dos_audio_change(100); CHECK(dos_audio_volume() == 100);
    CHECK(!pump(400) && last_left == 10000 && last_right == -10000);
    dos_audio_change(-50); dos_audio_mute(); CHECK(!dos_audio_volume());
    uint64_t before = submitted;
    CHECK(!pump(4000) && !remaining && !partial && !dos_audio_partial);
    CHECK(submitted-before < 1024 && !last_left && !last_right);
    unsigned before_calls = calls;
    for (unsigned i = 0; i < 120; ++i) CHECK(!tick(100,10,i));
    CHECK(calls == before_calls && !remaining);
    dos_audio_mute(); CHECK(dos_audio_volume() == 50);
    CHECK(!pump(512) && last_left == 5000 && last_right == -5000);
    dos_audio_change(-100); CHECK(!dos_audio_volume());
    CHECK(!pump(512) && !partial);
    dos_audio_change(10); CHECK(dos_audio_volume() == 10);
    CHECK(!pump(512) && last_left == 1000 && last_right == -1000);
    reject = 1; CHECK(pump(1800) == -1 && dos_audio_failed() && !remaining && !dos_audio_partial);
    before_calls = calls;
    for (unsigned i = 0; i < 30; ++i) CHECK(!tick(100,10,i));
    CHECK(calls == before_calls && !remaining);
    reject = 0; dos_audio_change(10);
    CHECK(!pump(512) && !dos_audio_failed() && last_left == 2000 && last_right == -2000);
    CHECK(dos_audio_partial == partial);
    return 0;
}
static int short_sound(void) {
    reset(0);
    CHECK(!pump(16) && submitted == 16); silence = 1;
    for (unsigned i = 0; i < 180; ++i) CHECK(!tick(56,10,i));
    CHECK(nonzero > 0 && !partial && !dos_audio_partial);
    CHECK(submitted <= 16 + PC_AUDIO_HZ/10 + 512 + 1023);
    unsigned before = calls;
    for (unsigned i = 0; i < 30; ++i) CHECK(!tick(56,10,i));
    CHECK(calls == before && !remaining);
    return 0;
}
static int extremes(void) {
    reset(0); source_left = 32767; source_right = -32768;
    CHECK(!pump(512) && last_left == 26213 && last_right == -26214);
    dos_audio_change(100); CHECK(!pump(512));
    CHECK(maximum == 32767 && minimum == -32768);
    return 0;
}
static int transitions(void) {
    static const unsigned speeds[] = {100,56,79,100};
    reset(0); source_pattern = verify_pcm = 1; dos_audio_level = 100;
    for (unsigned section = 0; section < 4; ++section)
        for (unsigned i = 0; i < 600; ++i) CHECK(!tick(speeds[section],10,i));
    CHECK(submitted == rendered && verified_frames == rendered && !mismatched_pcm && !blocked);
    printf("TRANSITIONS generated=%llu submitted=%llu bitexact=%llu blocked=%llu missing=%llu\n",
        (unsigned long long)rendered,(unsigned long long)submitted,(unsigned long long)verified_frames,
        (unsigned long long)blocked,(unsigned long long)missing);
    return 0;
}
static int bounded_work(void) {
    reset(0); source_pattern = verify_pcm = 1; dos_audio_level = 100;
    for (unsigned i = 0; i < 300; ++i) CHECK(!tick(56,10,i));
    advance(PC_AUDIO_HZ * 3 / 4);
    uint64_t before = submitted;
    CHECK(!pump(368) && submitted-before == 368 && !mismatched_pcm);
    injected_pause = PC_AUDIO_HZ / 5; /* 200 ms inside the audio callback */
    before = submitted;
    CHECK(!pump(368) && submitted-before == 368);
    CHECK(!pump(368) && submitted-before == 736 && !mismatched_pcm);
    before = submitted;
    unsigned before_calls = calls;
    advance(PC_AUDIO_HZ);
    CHECK(!pump(0) && submitted == before && calls == before_calls);
    printf("SLOW_CALLBACK submitted=%llu generated=%llu bitexact=%llu\n",
        (unsigned long long)submitted,(unsigned long long)rendered,(unsigned long long)verified_frames);
    reset(0); source_pattern = verify_pcm = 1; dos_audio_level = 100;
    expected_sequence = 8192-1024;
    CHECK(!pump(8192) && rendered == 8192 && submitted == 1024);
    CHECK(!remaining && !mismatched_pcm && verified_frames == 1024 && !blocked);
    /* Force a full host queue; accepting one more block must really block. */
    verify_pcm = 0;
    CHECK(!pump(1024));
    before = wall;
    CHECK(!pump(1024) && blocked == 1024 && wall-before == 1024);
    before = submitted;
    CHECK(!pump(368) && submitted-before == 368); /* no catch-up after waiting */
    printf("BOUNDED_WORK backlog_drained=8192 backlog_submitted=1024 max_guest_per_pump=%u blocked_frames=%llu\n",
        DOS_AUDIO_BUDGET,(unsigned long long)blocked);
    return 0;
}
static int progression(void) {
    reset(0); source_pattern = verify_pcm = 1; dos_audio_level = 100;
    for (unsigned i = 0; i < 600; ++i) CHECK(!tick(56,10,i));
    CHECK(!mismatched_pcm && !blocked && submitted == rendered);
    CHECK(changes > played * 9 / 10 && positive > played / 3 && negative > played / 3);
    printf("PROGRESSION played=%llu changed=%llu positive=%llu negative=%llu\n",
        (unsigned long long)played,(unsigned long long)changes,(unsigned long long)positive,(unsigned long long)negative);
    return 0;
}

int main(void) {
    CHECK(!controls()); CHECK(!short_sound()); CHECK(!extremes()); CHECK(!progression());
    CHECK(!steady(56,0,0)); CHECK(!steady(79,0,0)); CHECK(!steady(100,0,0));
    CHECK(!steady(56,1,0)); CHECK(!steady(79,1,0)); CHECK(!steady(100,1,0));
    CHECK(!steady(56,2,0)); CHECK(!steady(56,1,UINT32_MAX-500));
    CHECK(!transitions()); CHECK(!bounded_work());
    printf("AUDIO OUTPUT PASS: checks=%u fixed-rate/exact-PCM/bounded-backlog/mute/retry/slow-callback/56-79-100-percent/jitter/wrap\n",checks);
    return 0;
}
