/* Host test for the production x86 audio frontend's output contract. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static uint32_t clock_ms;
static unsigned mixed, submitted, calls, sink_delay, mix_delay;
static int fail_sink, signal_value = 1000, last_sample;
static uint32_t millis_stub(void) { return clock_ms; }
static int sink(const int16_t *data, unsigned frames, unsigned rate) {
    if (rate != 44100u || frames > 512u) abort();
    ++calls; clock_ms += sink_delay;
    if (fail_sink) return -1;
    submitted += frames; last_sample = data[0]; return 0;
}
static struct test_api {
    struct { uint32_t (*millis)(void); } base;
    int (*audio_pcm)(const int16_t *,unsigned,unsigned);
} api = {{millis_stub},sink}, *tab5_api = &api;
#include "../app_audio.h"
void machine_audio_render(int16_t *samples,unsigned frames) {
    if (frames > 512u) abort();
    mixed += frames;
    clock_ms += mix_delay;
    for (unsigned i = 0; i < frames * 2u; ++i) samples[i] = (int16_t)signal_value;
}
static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void) {
    dos_audio_start();
    CHECK(dos_audio_pump() == 0 && mixed == 0 && calls == 0);
    clock_ms = 10;
    CHECK(dos_audio_pump() == 0);
    CHECK(mixed == 441 && submitted == 441 && last_sample == 800);
    clock_ms += 1000;
    unsigned before = mixed;
    CHECK(dos_audio_pump() == 0 && mixed - before == DOS_AUDIO_BUDGET);
    dos_audio_mute(); before = mixed; clock_ms += 10;
    CHECK(dos_audio_pump() == 0 && mixed - before == 441);
    CHECK(last_sample == 0 && dos_audio_partial == 0);
    unsigned before_calls = calls; clock_ms += 10;
    CHECK(dos_audio_pump() == 0 && calls == before_calls);
    dos_audio_mute(); fail_sink = 1; clock_ms += 10;
    CHECK(dos_audio_pump() == -1 && dos_audio_failed());
    before_calls = calls; clock_ms += 10;
    CHECK(dos_audio_pump() == 0 && calls == before_calls);
    fail_sink = 0; dos_audio_change(10); clock_ms += 10;
    CHECK(dos_audio_pump() == 0 && !dos_audio_failed() && last_sample == 900);
    sink_delay = 1000; clock_ms += 10;
    CHECK(dos_audio_pump() == 0);
    before = mixed;
    CHECK(dos_audio_pump() == 0 && mixed == before);
    sink_delay = 0; mix_delay = 10; clock_ms += 10;
    CHECK(dos_audio_pump() == 0);
    before = mixed;
    CHECK(dos_audio_pump() == 0 && mixed - before == 441);
    puts("PASS x86 bounded audio, volume, mute, failure/retry and sink-clock feedback");
    printf("%d checks\n",checks); return 0;
}
