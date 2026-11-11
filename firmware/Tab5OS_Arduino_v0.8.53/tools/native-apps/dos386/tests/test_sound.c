/* Production SB16 implementation is included so ring-boundary invariants can
 * be exercised as well as real DSP ports and the real DMA controller. */
#ifdef _MSC_VER
static int host_ctz(unsigned value)
{ int n = 0; while (value && !(value & 1u)) { ++n; value >>= 1; } return n; }
#define __builtin_ctz host_ctz
#endif
#include "../core/sb16.c"
#include "../core/adlib.h"

static unsigned checks, errors, interrupts;
#define CHECK(e) do { ++checks; if (!(e)) { ++errors; fprintf(stderr, "FAIL %d: %s\n", __LINE__, #e); } } while (0)
static void irq(void *p, int n, int level) { (void)p; (void)n; interrupts += level != 0; }

static void seed(SB16State *s, int fmt, int stereo, int rate)
{
    memset(s, 0, sizeof(*s));
    s->fmt = (AudioFormat)fmt; s->fmt_stereo = stereo; s->freq = rate;
    s->active_out = 1;
    /* Deliberately unaligned, with a full ring wrapping through byte zero. */
    s->audio_p = AUDIO_BUF_LEN - 13; s->audio_q = s->audio_p + AUDIO_BUF_LEN;
    for (unsigned i = 0; i < AUDIO_BUF_LEN; ++i)
        s->audio_buf[(s->audio_p + i) % AUDIO_BUF_LEN] = (uint8_t)(i * 19 + 37);
}

static void chunk_test(void)
{
    const int rates[] = {4000, 8000, 11025, 22050, 32000, 44100, 48000, 65535};
    const unsigned chunks[] = {1, 3, 17, 64, 5, 127};
    for (int fmt = 0; fmt < 4; ++fmt) for (int stereo = 0; stereo < 2; ++stereo)
        for (unsigned r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r) {
            SB16State a, b; int16_t one[1024] = {0}, split[1024] = {0};
            seed(&a, fmt, stereo, rates[r]); b = a;
            sb16_audio_callback(&a, (uint8_t *)one, sizeof(one));
            unsigned at = 0, part = 0;
            while (at < 512) {
                unsigned n = chunks[part++ % (sizeof(chunks) / sizeof(chunks[0]))];
                if (n > 512 - at) n = 512 - at;
                sb16_audio_callback(&b, (uint8_t *)(split + at * 2), (int)n * 4);
                at += n;
            }
            CHECK(!memcmp(one, split, sizeof(one)));
            CHECK(a.audio_p == b.audio_p && a.audio_phase == b.audio_phase);
            unsigned bytes = (fmt >= 2 ? 2u : 1u) * (stereo ? 2u : 1u);
            CHECK(a.audio_p == AUDIO_BUF_LEN - 13 + (512u * (unsigned)rates[r] / 44100u) * bytes);
            CHECK(a.audio_phase == 512u * (unsigned)rates[r] % 44100u);
        }
}

static void format_test(void)
{
    for (int fmt = 0; fmt < 2; ++fmt) {
        SB16State s; int16_t out[512] = {0}; seed(&s, fmt, 0, 44100);
        for (unsigned i = 0; i < 256; ++i) s.audio_buf[(s.audio_p + i) % AUDIO_BUF_LEN] = (uint8_t)i;
        sb16_audio_callback(&s, (uint8_t *)out, sizeof(out));
        for (int i = 0; i < 256; ++i) {
            int expected = fmt == AUDIO_FORMAT_U8 ? (i - 128) * 256 : (i < 128 ? i : i - 256) * 256;
            CHECK(out[i * 2] == expected && out[i * 2 + 1] == expected);
        }
    }
    const unsigned words[] = {0, 1, 32767, 32768, 65534, 65535};
    for (int fmt = 2; fmt < 4; ++fmt) {
        SB16State s; int16_t out[12] = {0}; seed(&s, fmt, 1, 44100);
        s.audio_p = AUDIO_BUF_LEN - 1; s.audio_q = s.audio_p + 24;
        for (unsigned i = 0; i < 12; ++i) {
            unsigned word = words[i % 6];
            s.audio_buf[(s.audio_p + i * 2) % AUDIO_BUF_LEN] = word & 255;
            s.audio_buf[(s.audio_p + i * 2 + 1) % AUDIO_BUF_LEN] = word >> 8;
        }
        sb16_audio_callback(&s, (uint8_t *)out, sizeof(out));
        for (unsigned i = 0; i < 12; ++i) {
            unsigned word = words[i % 6];
            int expected = fmt == AUDIO_FORMAT_U16 ? (int)word - 32768 : word < 32768 ? (int)word : (int)word - 65536;
            CHECK(out[i] == expected);
        }
    }
}

static void dsp(SB16State *s, unsigned value) { sb16_dsp_write(s, 0x22c, value); }
static void dma_test(void)
{
    uint8_t ram[64]; for (unsigned i = 0; i < sizeof(ram); ++i) ram[i] = (uint8_t)(i * 7 + 1);
    I8257State *low = i8257_new((char *)ram, sizeof(ram), 0, 0x80, -1, 0);
    I8257State *high = i8257_new((char *)ram, sizeof(ram), 0xc0, 0x88, -1, 1);
    SB16State *s = sb16_new(0x220, 5, low, high, NULL, irq);
    sb16_dsp_write(s, 0x226, 1); sb16_dsp_write(s, 0x226, 0);
    CHECK(sb16_dsp_read(s, 0x22a) == 0xaa);
    sb16_mixer_write_indexb(s, 0x224, 0x81);
    sb16_mixer_write_datab(s, 0x225, 0); /* No selected channel is a valid probe. */
    CHECK(s->dma == 1 && s->hdma == 5);
    int16_t out[256] = {0}; sb16_audio_callback(s, (uint8_t *)out, sizeof(out));
    low->regs[1].mode = 0x48; low->regs[1].now[0] = 60; low->regs[1].base[1] = 15;
    dsp(s, 0x41); dsp(s, 0xac); dsp(s, 0x44); /* 44100 Hz */
    dsp(s, 0xc0); dsp(s, 0x10); dsp(s, 15); dsp(s, 0); /* signed 8-bit */
    i8257_dma_run(low);
    sb16_audio_callback(s, (uint8_t *)out, sizeof(out));
    CHECK(interrupts > 0);
    for (unsigned i = 0; i < 16; ++i) {
        int value = i < 4 ? ram[60 + i] : 255;
        int expected = (value < 128 ? value : value - 256) * 256;
        CHECK(out[i * 2] == expected && out[i * 2 + 1] == expected);
    }
    CHECK(SB_read_DMA(s, 1, -1, 16) == 0);
    CHECK(SB_read_DMA(s, 1, 16, 16) == 0);
    CHECK(SB_read_DMA(s, 1, 0, 0) == 0);
    CHECK(write_audio(s, 1, 0, 0, 1) == 0);
    CHECK(write_audio(s, 1, -1, 16, 1) == 0);
    CHECK(write_audio(s, 1, 0, 16, -1) == 0);
    /* Reset must discard pending samples and fractional source position. */
    s->audio_q = s->audio_p + 20; s->audio_phase = 12345;
    sb16_dsp_write(s, 0x226, 1); sb16_dsp_write(s, 0x226, 0);
    CHECK(s->audio_p == s->audio_q && !s->audio_phase && !s->audio_free);
    memset(out, 0, sizeof(out)); sb16_audio_callback(s, (uint8_t *)out, sizeof(out));
    int nonzero = 0; for (unsigned i = 0; i < 256; ++i) nonzero |= out[i]; CHECK(!nonzero);
    free(s); free(high); free(low);
}

static void edge_test(void)
{
    SB16State s; int16_t guarded[12]; seed(&s, AUDIO_FORMAT_S16, 1, 48000);
    for (unsigned i = 0; i < 12; ++i) guarded[i] = 1234;
    sb16_audio_callback(&s, (uint8_t *)(guarded + 2), 3);
    CHECK(s.audio_p == AUDIO_BUF_LEN - 13 && guarded[2] == 1234);
    s.freq = 0; sb16_audio_callback(&s, (uint8_t *)(guarded + 2), 16);
    CHECK(guarded[2] == 1234);
    s.freq = -1; sb16_audio_callback(&s, (uint8_t *)(guarded + 2), 16);
    CHECK(guarded[2] == 1234);
    s.freq = 44100; s.audio_q = s.audio_p + 5;
    sb16_audio_callback(&s, (uint8_t *)(guarded + 2), 16);
    CHECK(guarded[0] == 1234 && guarded[1] == 1234 && guarded[10] == 1234 && guarded[11] == 1234);
    CHECK(s.audio_q - s.audio_p == 1 && guarded[4] == 1234);
    s.audio_q = s.audio_p + AUDIO_BUF_LEN + 1;
    sb16_audio_callback(&s, (uint8_t *)(guarded + 2), 16);
    CHECK(s.audio_p == s.audio_q && !s.audio_phase);
}

static void opl_reg(AdlibState *s, unsigned reg, unsigned value)
{ adlib_write(s, 0x388, reg); adlib_write(s, 0x389, value); }
static void adlib_test(void)
{
    AdlibState *s = adlib_new(); CHECK(s != NULL); if (!s) return;
    int16_t out[514]; memset(out, 0, sizeof(out));
    adlib_callback(s, (uint8_t *)(out + 1), 1024);
    int nonzero = 0; for (unsigned i = 1; i < 513; ++i) nonzero |= out[i]; CHECK(!nonzero);
    opl_reg(s, 0x04, 0x60); opl_reg(s, 0x04, 0x80); CHECK((adlib_read(s, 0x388) & 0xe0) == 0);
    opl_reg(s, 0x02, 0xff); opl_reg(s, 0x04, 0x21); CHECK((adlib_read(s, 0x388) & 0xc0) == 0xc0);
    opl_reg(s, 0x04, 0x80);
    opl_reg(s, 0x20, 0x01); opl_reg(s, 0x23, 0x01);
    opl_reg(s, 0x40, 0x10); opl_reg(s, 0x43, 0x00);
    opl_reg(s, 0x60, 0xf0); opl_reg(s, 0x63, 0xf0);
    opl_reg(s, 0x80, 0x77); opl_reg(s, 0x83, 0x77);
    opl_reg(s, 0xc0, 0x0e); opl_reg(s, 0xa0, 0x98); opl_reg(s, 0xb0, 0x31);
    int positive = 0, negative = 0;
    for (unsigned block = 0; block < 8; ++block) {
        out[0] = 1234; out[513] = -1234;
        adlib_callback(s, (uint8_t *)(out + 1), 1024);
        CHECK(out[0] == 1234 && out[513] == -1234);
        for (unsigned i = 1; i < 513; ++i) { positive |= out[i] > 0; negative |= out[i] < 0; }
    }
    CHECK(positive && negative);
    adlib_free(s);
}

int main(void)
{
    chunk_test(); format_test(); dma_test(); edge_test(); adlib_test();
    printf("Sound checks: %u; failures: %u\n", checks, errors);
    return errors ? 1 : 0;
}
