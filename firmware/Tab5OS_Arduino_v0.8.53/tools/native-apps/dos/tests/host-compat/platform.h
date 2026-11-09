#pragma once
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(x)
#define __builtin_expect(x,expect) (x)
#define __ATOMIC_RELAXED 0
#define __ATOMIC_ACQUIRE 0
#define __ATOMIC_RELEASE 0
#define __ATOMIC_ACQ_REL 0
#define __atomic_load_n(p,order) (*(p))
#define __atomic_store_n(p,v,order) (*(p)=(v))
#define __atomic_fetch_add(p,v,order) ((*(p)+=(v))-(v))
#include <stdint.h>
static inline uint32_t host_atomic_exchange(volatile uint32_t *p,uint32_t value){uint32_t old=*p;*p=value;return old;}
static inline uint32_t host_atomic_or(volatile uint32_t *p,uint32_t value){uint32_t old=*p;*p|=value;return old;}
static inline int host_atomic_cas(volatile uint32_t *p,uint32_t *expected,uint32_t desired){if(*p==*expected){*p=desired;return 1;}*expected=*p;return 0;}
#define __atomic_exchange_n(p,v,order) host_atomic_exchange((volatile uint32_t*)(p),(uint32_t)(v))
#define __atomic_fetch_or(p,v,order) host_atomic_or((volatile uint32_t*)(p),(uint32_t)(v))
#define __atomic_compare_exchange_n(p,old,v,weak,s,f) host_atomic_cas((volatile uint32_t*)(p),(uint32_t*)(old),(uint32_t)(v))
#endif
#if defined(_WIN32)
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
/* Host harness allocations live for the duration of this one process. */
static inline int posix_memalign(void **out,size_t alignment,size_t size) {
 void *base=malloc(size+alignment-1);if(!base)return 12;
 *out=(void*)(((uintptr_t)base+alignment-1)&~(uintptr_t)(alignment-1));return 0;
}
static inline struct tm *localtime_r(const time_t *t,struct tm *out) {
 struct tm *value=localtime(t);if(!value)return NULL;*out=*value;return out;
}
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode)&S_IFMT)==S_IFDIR)
#define S_ISREG(mode) (((mode)&S_IFMT)==S_IFREG)
#endif
#endif
