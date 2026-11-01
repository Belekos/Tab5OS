#pragma once
#include <stddef.h>
#include <stdint.h>
enum { MALLOC_CAP_SPIRAM=1, MALLOC_CAP_INTERNAL=2, MALLOC_CAP_8BIT=4 };
void* heap_caps_malloc(size_t bytes, uint32_t caps);
void heap_caps_free(void* ptr);
