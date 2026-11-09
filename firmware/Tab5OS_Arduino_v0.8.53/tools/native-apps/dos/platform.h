#pragma once
#include "Tab5AppApi.h"
#include <stdint.h>
#include <stddef.h>
extern const struct tab5_app_api7 *dos_api;
extern char dos_mount_root[256];
struct folder;
void dos_overlay_init(uint32_t identity);
int dos_overlay_load(void);
int dos_overlay_save(void);
void dos_overlay_apply(int drive,uint32_t lba,uint32_t count,uint8_t *bytes);
int dos_overlay_write(int drive,uint32_t lba,uint32_t count,const uint8_t *bytes);
int dos_overlay_dirty(void);
uint32_t dos_folder_fingerprint(struct folder *folder);
int dos_folder_name(struct folder *folder,const char *filename,char result[13]);
