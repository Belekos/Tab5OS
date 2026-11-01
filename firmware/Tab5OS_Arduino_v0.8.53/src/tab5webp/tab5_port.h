#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void* Tab5WebPMalloc(size_t size);
void* Tab5WebPCalloc(size_t count, size_t size);
void Tab5WebPFree(void* bytes);
int Tab5WebPCheckpoint(void);
#ifdef __cplusplus
}
#endif
