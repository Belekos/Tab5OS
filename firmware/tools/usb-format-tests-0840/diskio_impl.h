#include <assert.h>
#include "ff.h"
#include "diskio.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NOT_FOUND 2
typedef struct {DSTATUS(*init)(BYTE);DSTATUS(*status)(BYTE);DRESULT(*read)(BYTE,BYTE*,DWORD,UINT);DRESULT(*write)(BYTE,const BYTE*,DWORD,UINT);DRESULT(*ioctl)(BYTE,BYTE,void*);} ff_diskio_impl_t;
esp_err_t ff_diskio_get_drive(BYTE*);
void ff_diskio_register(BYTE,const ff_diskio_impl_t*);
#define ff_diskio_unregister(p) ff_diskio_register(p,NULL)
