#pragma once
#if defined(_MSC_VER)
#include <stdint.h>
#include <time.h>
#include <math.h>
#define __wasm__ 1
#define __builtin_expect(value, expected) (value)
#define CLOCK_MONOTONIC 0
#define clock_gettime(clock_id, output) timespec_get(output, TIME_UTC)
static inline int usleep(unsigned usec){(void)usec;return 0;}
static inline int tab5_clzll(uint64_t v){int n=0;if(!v)return 64;while(!(v&(UINT64_C(1)<<63))){v<<=1;n++;}return n;}
static inline int tab5_ffsll(uint64_t v){int n=1;if(!v)return 0;while(!(v&1)){v>>=1;n++;}return n;}
#define __builtin_clzll(v) tab5_clzll(v)
#define __builtin_ffsll(v) tab5_ffsll(v)
static inline int tab5_ctz(uint32_t v){return v?tab5_ffsll(v)-1:32;}
#define __builtin_ctz(v) tab5_ctz(v)
static inline void sincos(double x,double *s,double *c){*s=sin(x);*c=cos(x);}
#endif
