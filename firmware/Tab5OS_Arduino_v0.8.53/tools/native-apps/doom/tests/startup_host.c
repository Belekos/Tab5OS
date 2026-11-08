/* Complete Doom engine startup through mocked Tab5 services. Test files only. */
#include "Tab5AppApi.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *files[8];
static void *allocations[32];
static size_t allocation_sizes[32], live_bytes, peak_bytes, pixels;
static const char *wad_path, *save_directory, *scenario;
static unsigned frames, audios, saves, clock_ms, nonzero_frames;
static uint32_t frame_hash;
static unsigned target_frames;

static void log_text(const char *text, size_t bytes) { fwrite(text, 1, bytes, stdout); }
static int stopped(void) { return frames >= target_frames; }
static int sleep_ms(uint32_t ms) { clock_ms += ms ? ms : 1; return stopped(); }
static uint32_t millis_fn(void) { return ++clock_ms; }
static void *allocate(size_t bytes)
{
    if (!strcmp(scenario, "oom")) return NULL;
    assert(bytes && bytes <= 16u * 1024 * 1024 && live_bytes + bytes <= 20u * 1024 * 1024);
    for (unsigned i = 0; i < 32; ++i) if (!allocations[i]) {
        allocations[i] = calloc(1, bytes);
        assert(allocations[i]); allocation_sizes[i] = bytes; live_bytes += bytes;
        if (live_bytes > peak_bytes) peak_bytes = live_bytes;
        return allocations[i];
    }
    return NULL;
}
static void release(void *pointer)
{
    if (!pointer) return;
    for (unsigned i = 0; i < 32; ++i) if (allocations[i] == pointer) {
        free(pointer); allocations[i] = NULL; live_bytes -= allocation_sizes[i]; return;
    }
    assert(!"Unknown allocation");
}
static void resolve(char *out, size_t capacity, const char *path)
{
    if (!strcmp(path, "/Roms/doom1.wad")) snprintf(out, capacity, "%s", wad_path);
    else if (!strncmp(path, "/Saves/Doom/", 12))
        snprintf(out, capacity, "%s/%s", save_directory, strrchr(path, '/') + 1);
    else out[0] = 0;
}
static int open_file(const char *path)
{
    char full[4096]; resolve(full, sizeof(full), path);
    if (!full[0] || !strcmp(scenario, "missing")) return -1;
    for (unsigned i = 0; i < 8; ++i) if (!files[i]) {
        files[i] = fopen(full, "rb"); return files[i] ? (int)i + 1 : -1;
    }
    return -1;
}
static int read_file(int handle, void *out, size_t bytes)
{
    assert(handle > 0 && handle <= 8 && files[handle - 1] && bytes <= 4096);
    return (int)fread(out, 1, bytes, files[handle - 1]);
}
static int seek_file(int handle, uint32_t offset) { return fseek(files[handle - 1], offset, SEEK_SET); }
static int close_file(int handle) { int result = fclose(files[handle - 1]); files[handle - 1] = NULL; return result; }
static uint32_t file_size(int handle)
{
    FILE *file = files[handle - 1]; long previous = ftell(file);
    fseek(file, 0, SEEK_END); long size = ftell(file); fseek(file, previous, SEEK_SET);
    return (uint32_t)size;
}
static int video_begin(uint16_t width, uint16_t height)
{
    assert(width == 320 && height == 200); pixels = (size_t)width * height;
    return !strcmp(scenario, "video-failure") ? -1 : 0;
}
static int present(const uint16_t *data, size_t count)
{
    assert(data && count == pixels); unsigned nonzero = 0; uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) { nonzero |= data[i]; hash = (hash ^ data[i]) * 16777619u; }
    frame_hash = hash; nonzero_frames += nonzero != 0; ++frames; return 0;
}
static uint32_t buttons(void)
{
    if (strcmp(scenario, "play")) return 0;
    /* Open the title menu, choose New Game, episode one, then default skill. */
    if ((frames >= 5 && frames < 7) || (frames >= 15 && frames < 17) ||
        (frames >= 25 && frames < 27) || (frames >= 35 && frames < 37)) return 128;
    if (frames == 150) return 512;
    if (frames >= 80 && frames < 110) return 4 | 16;
    if (frames >= 60 && frames < 120) return 4;
    return 0;
}
static int audio_pcm(const int16_t *samples, size_t count, uint32_t rate)
{
    assert(samples && count <= 2048 && rate >= 8000 && rate <= 48000);
    volatile int sum = 0; for (size_t i = 0; i < count * 2; ++i) sum ^= samples[i];
    (void)sum; ++audios; return 0;
}
static int save_file(const char *path, const void *data, size_t size)
{
    assert(size <= 2u * 1024 * 1024); char full[4096]; resolve(full, sizeof(full), path);
    assert(full[0]); FILE *file = fopen(full, "wb"); assert(file);
    assert(fwrite(data, 1, size, file) == size); assert(!fclose(file)); ++saves; return 0;
}
static uint32_t unix_time_fn(void) { return 1700000000; }
static uint64_t floating(unsigned op, uint64_t a, uint64_t b)
{
    union { uint64_t u; double d; } x = {.u = a}, y = {.u = b}, z;
    union { uint32_t u; float f; } f;
    switch (op) {
    case 0: z.d = x.d + y.d; break; case 1: z.d = x.d - y.d; break;
    case 2: z.d = x.d * y.d; break; case 3: z.d = x.d / y.d; break;
    case 4: z.d = (int32_t)a; break; case 5: z.d = (uint32_t)a; break;
    case 6: z.d = (double)(int64_t)a; break; case 7: z.d = (double)a; break;
    case 8: f.u = (uint32_t)a; z.d = f.f; break; case 9: f.f = (float)x.d; return f.u;
    case 10: return (uint64_t)(int64_t)(int32_t)x.d; case 11: return (uint32_t)x.d;
    case 12: return (uint64_t)(int64_t)x.d; case 13: return (uint64_t)x.d;
    case 20: z.d = sin(x.d); break; case 21: z.d = cos(x.d); break;
    case 22: z.d = tan(x.d); break; case 23: z.d = atan(x.d); break;
    case 24: z.d = atan2(x.d, y.d); break; case 25: z.d = acos(x.d); break;
    case 26: z.d = asin(x.d); break; case 27: z.d = sqrt(x.d); break;
    case 28: z.d = floor(x.d); break; case 29: z.d = ceil(x.d); break;
    case 30: z.d = fmod(x.d, y.d); break; case 31: z.d = pow(x.d, y.d); break;
    case 32: z.d = exp(x.d); break; case 33: z.d = log(x.d); break;
    case 34: z.d = ldexp(x.d, (int)b); break; default: abort();
    }
    return z.u;
}
static int calendar(uint32_t timestamp, int utc, int *fields)
{
    time_t seconds = timestamp; struct tm *t = utc ? gmtime(&seconds) : localtime(&seconds);
    if (!t) return -1;
    int values[] = {t->tm_sec, t->tm_min, t->tm_hour, t->tm_mday, t->tm_mon,
                    t->tm_year, t->tm_wday, t->tm_yday, t->tm_isdst};
    memcpy(fields, values, sizeof(values)); return 0;
}
static struct tab5_app_api4 api;
const struct tab5_app_api4 *tab5_app_api_v4(void)
{
    api.base.base = (struct tab5_app_api2) {
        {4, sizeof(api), "en", log_text, stopped, sleep_ms, allocate, release,
         open_file, read_file, seek_file, close_file, millis_fn},
        file_size, video_begin, present, buttons, audio_pcm, save_file};
    api.unix_time = unix_time_fn; api.floating = floating; api.calendar = calendar;
    return &api;
}
extern int tab5_app_main(int argc, char **argv);
int main(int argc, char **argv)
{
    assert(argc == 4); wad_path = argv[1]; save_directory = argv[2]; scenario = argv[3];
    target_frames = !strcmp(scenario, "play") ? 240 : 120;
    setbuf(stdout, NULL);
    char *app_argv[] = {"/Applications/Doom.app.elf", "/Roms/doom1.wad", NULL};
    int result = tab5_app_main(!strcmp(scenario, "no-argument") ? 1 : 2, app_argv);
    int expected = !strcmp(scenario, "oom") || !strcmp(scenario, "missing") ? 9 :
        !strcmp(scenario, "video-failure") ? 3 : !strcmp(scenario, "no-argument") ? 2 : 0;
    assert(result == expected);
    if (!expected) { assert(frames == target_frames && nonzero_frames && audios);
        if (!strcmp(scenario, "play")) assert(saves); }
    else assert(frames == 0);
    for (unsigned i = 0; i < 8; ++i) if (files[i]) close_file((int)i + 1);
    for (unsigned i = 0; i < 32; ++i) if (allocations[i]) release(allocations[i]);
    assert(live_bytes == 0);
    printf("{\"passed\":true,\"scenario\":\"%s\",\"result\":%d,\"frames\":%u,"
           "\"nonzeroFrames\":%u,\"audioCalls\":%u,\"saves\":%u,\"peakBytes\":%zu,"
           "\"lastFrameHash\":%u}\n", scenario, result, frames, nonzero_frames, audios,
           saves, peak_bytes, frame_hash);
    return 0;
}
