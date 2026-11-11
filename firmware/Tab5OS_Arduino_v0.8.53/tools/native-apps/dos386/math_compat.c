/* DOS x86 additions to common/math.c, compiled as PIC with the application.
 * IEEE binary32/binary64 are also required by the CPU's x87/SIMD storage.
 * The ABI does not expose fenv: rounding here is nearest/ties-to-even, as the
 * upstream core already assumes. Guest x87 directed rounding uses floor,
 * ceil or trunc separately; full MXCSR exception/rounding emulation is not
 * added here. No dependency on non-PIC libm tables or libgcc bit-scan tables. */
#include <stdint.h>
#include <math.h>

union dos_double_bits { double value; uint64_t bits; };
union dos_float_bits { float value; uint32_t bits; };
#define DOS_DSIGN UINT64_C(0x8000000000000000)
#define DOS_DFRAC UINT64_C(0x000fffffffffffff)

double copysign(double x, double y)
{
    union dos_double_bits a = {.value=x}, b = {.value=y};
    a.bits = (a.bits & ~DOS_DSIGN) | (b.bits & DOS_DSIGN);
    return a.value;
}

double frexp(double x, int *exponent)
{
    union dos_double_bits a = {.value=x};
    unsigned e = (unsigned)((a.bits >> 52) & 2047);
    uint64_t mantissa = a.bits & DOS_DFRAC;
    *exponent = 0;
    if (e == 2047 || (e == 0 && mantissa == 0)) return x;
    if (e) {
        *exponent = (int)e - 1022;
    } else {
        /* Normalize subnormals with integer shifts, without rounding them. */
        int n = -1021;
        while (!(mantissa & (UINT64_C(1) << 52))) {
            mantissa <<= 1;
            --n;
        }
        *exponent = n;
    }
    a.bits = (a.bits & DOS_DSIGN) | (UINT64_C(1022) << 52) | (mantissa & DOS_DFRAC);
    return a.value;
}

double trunc(double x)
{
    union dos_double_bits a = {.value=x};
    int e = (int)((a.bits >> 52) & 2047) - 1023;
    if (e < 0) a.bits &= DOS_DSIGN;
    else if (e < 52) a.bits &= ~((UINT64_C(1) << (52-e)) - 1);
    return a.value;
}

static double dos_round_even(double x)
{
    union dos_double_bits a = {.value=x};
    int e = (int)((a.bits >> 52) & 2047) - 1023;
    if (e >= 52) return x; /* Already integral, infinity or NaN. */
    if (e < 0) {
        uint64_t magnitude = a.bits & ~DOS_DSIGN;
        a.bits &= DOS_DSIGN;
        if (magnitude > UINT64_C(0x3fe0000000000000))
            a.bits |= UINT64_C(0x3ff0000000000000);
    } else {
        uint64_t unit = UINT64_C(1) << (52-e);
        uint64_t remainder = a.bits & (unit-1);
        a.bits &= ~(unit-1);
        if (remainder > (unit >> 1) ||
            (remainder == (unit >> 1) && (a.bits & unit))) a.bits += unit;
    }
    return a.value;
}

static float dos_round_even_float(float x)
{
    union dos_float_bits a = {.value=x};
    int e = (int)((a.bits >> 23) & 255) - 127;
    if (e >= 23) return x;
    if (e < 0) {
        uint32_t magnitude = a.bits & UINT32_C(0x7fffffff);
        a.bits &= UINT32_C(0x80000000);
        if (magnitude > UINT32_C(0x3f000000)) a.bits |= UINT32_C(0x3f800000);
    } else {
        uint32_t unit = UINT32_C(1) << (23-e);
        uint32_t remainder = a.bits & (unit-1);
        a.bits &= ~(unit-1);
        if (remainder > (unit >> 1) ||
            (remainder == (unit >> 1) && (a.bits & unit))) a.bits += unit;
    }
    return a.value;
}

double nearbyint(double x) { return dos_round_even(x); }
float nearbyintf(float x) { return dos_round_even_float(x); }
double rint(double x) { return dos_round_even(x); }
float rintf(float x) { return dos_round_even_float(x); }

/* Newlib may expose log2(x) as a macro; parentheses keep this definition a
 * real function while preserving the host probe's object-like renaming. */
double (log2)(double x)
{
    union dos_double_bits a = {.value=x};
    const double inv_ln2 = 1.44269504088896340736;
    if ((a.bits & DOS_DSIGN) || a.bits == 0 ||
        ((a.bits >> 52) & 2047) == 2047) return log(x) * inv_ln2;
    int exponent;
    double mantissa = frexp(x, &exponent);
    /* Exact for powers of two, including the smallest positive subnormal. */
    return log(mantissa * 2.0) * inv_ln2 + (exponent-1);
}

double log10(double x) { return log(x) * 0.43429448190325182765; }
void sincos(double x, double *s, double *c) { *s = sin(x); *c = cos(x); }
double nan(const char *payload)
{
    union dos_double_bits a = {.bits=UINT64_C(0x7ff8000000000000)};
    (void)payload; /* The payload representation is implementation-defined. */
    return a.value;
}

/* GCC's RV32 helpers otherwise pull non-PIC __clz_tab references. The loops
 * deliberately use no clz/ffs builtins, which could call these same symbols. */
static int dos_clz32(uint32_t x)
{
    int n = 0;
    if (!x) return 32;
    if (!(x & UINT32_C(0xffff0000))) { n += 16; x <<= 16; }
    if (!(x & UINT32_C(0xff000000))) { n += 8; x <<= 8; }
    if (!(x & UINT32_C(0xf0000000))) { n += 4; x <<= 4; }
    if (!(x & UINT32_C(0xc0000000))) { n += 2; x <<= 2; }
    if (!(x & UINT32_C(0x80000000))) ++n;
    return n;
}

int __clzdi2(uint64_t x)
{
    uint32_t high = (uint32_t)(x >> 32);
    return high ? dos_clz32(high) : 32 + dos_clz32((uint32_t)x);
}

int __ffsdi2(uint64_t x)
{
    uint32_t word = (uint32_t)x;
    int offset = 0;
    if (!word) { word = (uint32_t)(x >> 32); offset = 32; }
    if (!word) return 0;
    /* Isolate the lowest set bit with unsigned arithmetic. */
    return offset + 32 - dos_clz32(word & (UINT32_C(0)-word));
}
