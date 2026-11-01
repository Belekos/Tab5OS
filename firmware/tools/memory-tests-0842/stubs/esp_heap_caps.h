#pragma once
#include <stddef.h>
#include <stdint.h>
#define MALLOC_CAP_8BIT 1U
#define MALLOC_CAP_INTERNAL 2U
#define MALLOC_CAP_DMA 4U
#define MALLOC_CAP_SPIRAM 8U
#define MALLOC_CAP_TCM 16U
#define MALLOC_CAP_RTCRAM 32U
void* heap_caps_malloc(size_t,uint32_t);
void* heap_caps_calloc(size_t,size_t,uint32_t);
void* heap_caps_realloc(void*,size_t,uint32_t);
void heap_caps_free(void*);
size_t heap_caps_get_free_size(uint32_t);
size_t heap_caps_get_minimum_free_size(uint32_t);
size_t heap_caps_get_largest_free_block(uint32_t);

size_t heap_caps_get_total_size(uint32_t);
