#pragma once
#include <stddef.h>
#include <stdint.h>
#define MALLOC_CAP_8BIT 1U
#define MALLOC_CAP_SPIRAM 8U
void* heap_caps_malloc(size_t,uint32_t);
void heap_caps_free(void*);
