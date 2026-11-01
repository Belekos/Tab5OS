#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MEM_ANY 0
#define MEM_FAST 0
#define MEM_SLOW 0
#define MEM_8BIT 0
#define MEM_NOPANIC 0
#define RG_LOGI(...) printf(__VA_ARGS__)
#define RG_LOGW(...) printf(__VA_ARGS__)
#define RG_LOGE(...) printf(__VA_ARGS__)
#define RG_LOGD(...) ((void)0)
#define RG_PANIC(...) abort()
#define rg_alloc(n,flags) calloc(1,n)
#define rg_free free
