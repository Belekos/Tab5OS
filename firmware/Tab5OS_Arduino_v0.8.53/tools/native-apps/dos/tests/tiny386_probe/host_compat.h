#pragma once
#include <stdint.h>
#include <time.h>
#include <io.h>
#define __wasm__ 1
#define BPP 16
#define CLOCK_MONOTONIC 0
#define clock_gettime(clock_id, output) timespec_get(output, TIME_UTC)
#define __builtin_expect(value, expected) (value)
static inline unsigned tab5_ctz(unsigned v) { unsigned n=0; if(!v)return 32; while(!(v&1)){v>>=1;n++;}return n; }
#define __builtin_ctz(value) tab5_ctz(value)
#define fseeko _fseeki64
#define ftello _ftelli64
#define write _write
#define read _read
