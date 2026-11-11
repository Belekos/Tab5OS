/* Compare the application algorithms with host libm without interposing on
 * libm itself (which may internally call frexp, log2, etc.). */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

static double reference_frexp(double x, int *e) { return frexp(x,e); }
static double reference_trunc(double x) { return trunc(x); }
static double reference_nearest(double x) { return nearbyint(x); }
static float reference_nearest_float(float x) { return nearbyintf(x); }
static double reference_log2(double x) { return log2(x); }
static double reference_log10(double x) { return log10(x); }
#define frexp dos_test_frexp
#define trunc dos_test_trunc
#define nearbyint dos_test_nearbyint
#define nearbyintf dos_test_nearbyintf
#define rint dos_test_rint
#define rintf dos_test_rintf
#define log2 dos_test_log2
#define log10 dos_test_log10
#define copysign dos_test_copysign
#define sincos dos_test_sincos
#define nan dos_test_nan
#include "../math_compat.c"

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static uint64_t bits(double x) { union dos_double_bits u={.value=x}; return u.bits; }
static uint32_t fbits(float x) { union dos_float_bits u={.value=x}; return u.bits; }

static void compare(uint64_t pattern)
{
    union dos_double_bits x={.bits=pattern};
    unsigned e=(unsigned)((pattern>>52)&2047);
    int actual_exp=999,expected_exp=999;
    double actual=frexp(x.value,&actual_exp);
    if(e==2047) {
        CHECK(bits(actual)==pattern && actual_exp==0);
        CHECK(bits(nearbyint(x.value))==pattern);
        CHECK(bits(trunc(x.value))==pattern);
    } else {
        double expected=reference_frexp(x.value,&expected_exp);
        CHECK(bits(actual)==bits(expected));
        CHECK(actual_exp==expected_exp);
        CHECK(bits(trunc(x.value))==bits(reference_trunc(x.value)));
        CHECK(bits(nearbyint(x.value))==bits(reference_nearest(x.value)));
        CHECK(bits(rint(x.value))==bits(reference_nearest(x.value)));
    }
    union dos_float_bits f={.bits=(uint32_t)pattern};
    if(((f.bits>>23)&255)==255) {
        CHECK(fbits(nearbyintf(f.value))==f.bits);
    } else {
        CHECK(fbits(nearbyintf(f.value))==fbits(reference_nearest_float(f.value)));
        CHECK(fbits(rintf(f.value))==fbits(reference_nearest_float(f.value)));
    }
}

int main(void)
{
    static const uint64_t patterns[]={
        0,UINT64_C(0x8000000000000000),1,UINT64_C(0x000fffffffffffff),
        UINT64_C(0x0010000000000000),UINT64_C(0x7fefffffffffffff),
        UINT64_C(0x7ff0000000000000),UINT64_C(0xfff0000000000000),
        UINT64_C(0x7ff8000000000123),UINT64_C(0xfff8000000000123),
        UINT64_C(0x3fdfffffffffffff),UINT64_C(0x3fe0000000000000),UINT64_C(0x3fe0000000000001),
        UINT64_C(0xbfdfffffffffffff),UINT64_C(0xbfe0000000000000),UINT64_C(0xbfe0000000000001),
        UINT64_C(0x3ff8000000000000),UINT64_C(0x4004000000000000),
        UINT64_C(0xbff8000000000000),UINT64_C(0xc004000000000000),
        UINT64_C(0x432fffffffffffff),UINT64_C(0x4330000000000000),
        UINT64_C(0x000000003f000000),UINT64_C(0x00000000bf000000),
        UINT64_C(0x000000003fc00000),UINT64_C(0x0000000040200000),
        UINT64_C(0x00000000bfc00000),UINT64_C(0x00000000c0200000)
    };
    for(unsigned i=0;i<sizeof(patterns)/sizeof(patterns[0]);i++) compare(patterns[i]);
    uint64_t state=UINT64_C(0x7132fabc01234567);
    for(unsigned i=0;i<4096;i++) {
        state^=state<<13; state^=state>>7; state^=state<<17; compare(state);
    }
    CHECK(__clzdi2(0)==64 && __ffsdi2(0)==0);
    CHECK(__clzdi2(UINT64_MAX)==0 && __ffsdi2(UINT64_MAX)==1);
    for(unsigned i=0;i<64;i++) {
        uint64_t bit=UINT64_C(1)<<i;
        CHECK(__clzdi2(bit)==63-(int)i);
        CHECK(__ffsdi2(bit)==(int)i+1);
        CHECK(__ffsdi2(bit|(bit-1))==1);
    }
    CHECK(bits(copysign(1.0,-0.0))==bits(-1.0));
    CHECK(bits(copysign(-0.0,1.0))==0);
    CHECK((bits(nan("123"))&UINT64_C(0x7ff8000000000000))==UINT64_C(0x7ff8000000000000));
    for(int e=-1074;e<=1023;e++) {
        double x=ldexp(1.0,e);
        CHECK(log2(x)==(double)e);
    }
    for(unsigned i=1;i<512;i++) {
        double x=(double)i/7.0;
        double a=log2(x),b=reference_log2(x);
        CHECK(fabs(a-b)<=8*DBL_EPSILON*fmax(1.0,fabs(b)));
        a=log10(x);b=reference_log10(x);
        CHECK(fabs(a-b)<=8*DBL_EPSILON*fmax(1.0,fabs(b)));
    }
    CHECK(isinf(log2(0.0)) && signbit(log2(0.0)));
    CHECK(isnan(log2(-1.0)));
    double s,c;sincos(0.75,&s,&c);
    CHECK(s==sin(0.75) && c==cos(0.75));
    printf("PASS DOS x86 math: %u checks\n",checks);
    return 0;
}
