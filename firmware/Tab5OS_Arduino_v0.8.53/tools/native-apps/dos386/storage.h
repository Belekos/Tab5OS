#pragma once
#include <stdint.h>
/* Virtual block devices only. Underlying SD files are never opened writable. */
int x86_storage_open(const char *folder, const char *argument, char program[13]);
uint32_t x86_storage_identity(void);
int x86_storage_has_mouse_driver(void);
void x86_storage_close(void);
uint32_t x86_storage_time_ms(void);
int x86_disk_open(const char *path, int readonly);
uint32_t x86_disk_sectors(int handle);
int x86_disk_readonly(int handle);
int x86_disk_read(int handle,uint32_t lba,uint32_t count,uint8_t *bytes);
int x86_disk_write(int handle,uint32_t lba,uint32_t count,const uint8_t *bytes);
void x86_disk_close(int handle);
void x86_disk_geometry(int handle,int *cylinders,int *heads,int *sectors);
