#pragma once
#include "Tab5AppApi.h"
#include <stdint.h>
#include <stddef.h>
extern const struct tab5_app_api2* tab5_api;
extern intptr_t tab5_escape[5];
void tab5_runtime_capacity(size_t bytes);
void tab5_runtime_init(const struct tab5_app_api2* api);
uint32_t tab5_crc32(uint32_t seed,const void* data,size_t bytes);
void tab5_save_name(char* out,const char* system,const char* ext,uint32_t crc);
int tab5_load(int handle,void* data,size_t size);
void tab5_message(const char* en,const char* fr);

/* Reclaim application-owned caches once before reporting allocation failure. */
void tab5_runtime_reclaimer(void (*release_caches)(void));
