// Uses unchanged ESP-IDF 5.5.4 ff.c, ffunicode.c and diskio.c and SDK ff.h/ffconf.
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "diskio_impl.h"
#define ESP_ERR_INVALID_ARG 3
#define ESP_ERR_NOT_SUPPORTED 4
#define ESP_ERR_NO_MEM 5
#define ESP_ERR_MSC_FORMAT_FAILED 6
#define ESP_ERR_MSC_MOUNT_FAILED 7
#define ESP_FAIL 8
#define ESP_ERR_INVALID_STATE 9
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,4)
#define ESP_LOGE(...) ((void)0)
#define MSC_RETURN_ON_FALSE(a,b) do{if(!(a))return (b);}while(0)
#define MSC_RETURN_ON_INVALID_ARG(a) MSC_RETURN_ON_FALSE(a,ESP_ERR_INVALID_ARG)
#define MSC_GOTO_ON_ERROR(a) do{int r=(a);if(r!=ESP_OK){ret=r;goto fail;}}while(0)
#define MSC_GOTO_ON_FALSE(a,b) do{if(!(a)){ret=(b);goto fail;}}while(0)
typedef struct {uint32_t block_size,block_count;} usb_disk_t;
typedef struct {usb_disk_t disk;} msc_device_t;
typedef msc_device_t* msc_host_device_handle_t;
typedef struct msc_host_vfs* msc_host_vfs_handle_t;
typedef struct {bool format_if_mount_failed;int max_files;size_t allocation_unit_size;} esp_vfs_fat_mount_config_t;
typedef struct {const char* base_path;const char* fat_drive;size_t max_files;} esp_vfs_fat_conf_t;
static unsigned checks;
#define CHECK(c) do{++checks;if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);abort();}}while(0)
static BYTE* storage;
static size_t storageBytes;
static msc_device_t device;
static bool plugged=true;
static int ioError=0;
static void* heap_caps_malloc(size_t size,unsigned caps){(void)caps;return malloc(size);}
static void* heap_caps_calloc(size_t n,size_t size,unsigned caps){(void)caps;return calloc(n,size);}
static void heap_caps_free(void* p){free(p);}
void* ff_memalloc(unsigned n){return malloc(n);}void ff_memfree(void* p){free(p);}
int ff_mutex_create(int vol){(void)vol;return 1;}void ff_mutex_delete(int vol){(void)vol;}
int ff_mutex_take(int vol){(void)vol;return 1;}void ff_mutex_give(int vol){(void)vol;}
static esp_err_t scsi_cmd_read10(msc_host_device_handle_t dev,BYTE* out,DWORD sec,UINT n,uint32_t size){CHECK(dev==&device);CHECK((uint64_t)(sec+n)*size<=storageBytes);if(!plugged||ioError)return ESP_FAIL;memcpy(out,storage+(size_t)sec*size,n*size);return ESP_OK;}
static esp_err_t scsi_cmd_write10(msc_host_device_handle_t dev,const BYTE* src,DWORD sec,UINT n,uint32_t size){CHECK(dev==&device);CHECK((uint64_t)(sec+n)*size<=storageBytes);if(!plugged||ioError)return ESP_FAIL;memcpy(storage+(size_t)sec*size,src,n*size);return ESP_OK;}
static esp_err_t tab5_scsi_sync_cache(msc_host_device_handle_t dev){CHECK(dev==&device);return plugged&&!ioError?ESP_OK:ESP_FAIL;}
static bool isPresent(void* context){(void)context;return plugged;}
#include "production_raw.c"
// Regular MSC callbacks remain alive independently from the temporary raw drive.
static DSTATUS msc_init(BYTE p){(void)p;return plugged?0:STA_NODISK;}
static DRESULT msc_read(BYTE p,BYTE* out,DWORD sec,UINT n){(void)p;return scsi_cmd_read10(&device,out,sec,n,device.disk.block_size)==ESP_OK?RES_OK:RES_ERROR;}
static DRESULT msc_write(BYTE p,const BYTE* out,DWORD sec,UINT n){(void)p;return scsi_cmd_write10(&device,out,sec,n,device.disk.block_size)==ESP_OK?RES_OK:RES_ERROR;}
static DRESULT msc_ioctl(BYTE p,BYTE cmd,void* out){(void)p;switch(cmd){case CTRL_SYNC:return RES_OK;case GET_SECTOR_COUNT:*(DWORD*)out=device.disk.block_count;return RES_OK;case GET_SECTOR_SIZE:*(WORD*)out=device.disk.block_size;return RES_OK;default:return RES_ERROR;}}
static const ff_diskio_impl_t mscCallbacks={msc_init,msc_init,msc_read,msc_write,msc_ioctl};
static void ff_diskio_register_msc(BYTE p,usb_disk_t* disk){CHECK(disk==&device.disk);ff_diskio_register(p,&mscCallbacks);}
static FATFS* registeredFs;
static esp_err_t esp_vfs_fat_register_cfg(const esp_vfs_fat_conf_t* cfg,FATFS** out){CHECK(!strcmp(cfg->base_path,"/sd/USB"));CHECK(!strcmp(cfg->fat_drive,"1:"));if(registeredFs)return ESP_ERR_INVALID_STATE;registeredFs=calloc(1,sizeof(FATFS));*out=registeredFs;return registeredFs?ESP_OK:ESP_ERR_NO_MEM;}
static esp_err_t esp_vfs_fat_unregister_path(const char* path){CHECK(!strcmp(path,"/sd/USB"));free(registeredFs);registeredFs=NULL;return ESP_OK;}
#include "production_vfs.c"
static void writeReadFile(void){FIL f;UINT n;char data[16]={0};CHECK(f_open(&f,"1:/after.txt",FA_CREATE_ALWAYS|FA_WRITE)==FR_OK);CHECK(f_write(&f,"mounted",7,&n)==FR_OK&&n==7);CHECK(f_close(&f)==FR_OK);CHECK(f_open(&f,"1:/after.txt",FA_READ)==FR_OK);CHECK(f_read(&f,data,sizeof(data),&n)==FR_OK&&n==7&&!strcmp(data,"mounted"));CHECK(f_close(&f)==FR_OK);}
int main(void){
 // Reserve drive 0 for SD, as on Tab5. Unknown GPT/FS bytes on USB drive 1.
 ff_diskio_register(0,&mscCallbacks);
 esp_vfs_fat_mount_config_t cfg={false,12,0};
 for(unsigned size=512;size<=4096;size*=2){
  storageBytes=(size_t)size*131072;storage=calloc(1,storageBytes);CHECK(storage);device.disk=(usb_disk_t){size,131072};
  memset(storage,0x76,size*34);memset(storage+storageBytes-size*33,0x98,size*33);
  msc_host_vfs_handle_t vfs=NULL;
  CHECK(msc_host_vfs_register(&device,"/sd/USB",&cfg,&vfs)==ESP_ERR_MSC_MOUNT_FAILED);CHECK(vfs==NULL&&!registeredFs);
  for(unsigned cycle=0;cycle<3;++cycle){
   CHECK(tab5_msc_format_raw(&device,true,isPresent,NULL)==ESP_OK);
   CHECK(!active);CHECK(VolToPart[1].pd==1&&VolToPart[1].pt==0);
   CHECK(msc_host_vfs_register(&device,"/sd/USB",&cfg,&vfs)==ESP_OK);CHECK(registeredFs->fs_type==FS_FAT32);writeReadFile();
   CHECK(msc_host_vfs_unregister(vfs)==ESP_OK);vfs=NULL;CHECK(!registeredFs);
   CHECK(msc_host_vfs_register(&device,"/sd/USB",&cfg,&vfs)==ESP_OK);writeReadFile();CHECK(msc_host_vfs_unregister(vfs)==ESP_OK);vfs=NULL;
  }
  if(size==512){CHECK(tab5_msc_format_raw(&device,false,isPresent,NULL)==ESP_OK);CHECK(msc_host_vfs_register(&device,"/sd/USB",&cfg,&vfs)==ESP_OK);CHECK(registeredFs->fs_type==FS_FAT16);writeReadFile();CHECK(msc_host_vfs_unregister(vfs)==ESP_OK);}
  ioError=1;CHECK(tab5_msc_format_raw(&device,true,isPresent,NULL)!=ESP_OK);CHECK(!active&&!registeredFs);ioError=0;
  plugged=false;CHECK(tab5_msc_format_raw(&device,true,isPresent,NULL)!=ESP_OK);plugged=true;
  CHECK(tab5_msc_format_raw(&device,true,isPresent,NULL)==ESP_OK);CHECK(msc_host_vfs_register(&device,"/sd/USB",&cfg,&vfs)==ESP_OK);writeReadFile();CHECK(msc_host_vfs_unregister(vfs)==ESP_OK);
  free(storage);
 }
 ff_diskio_unregister(0);
 printf("{\"passed\":true,\"checks\":%u,\"cycles\":12,\"sectorSizes\":[512,1024,2048,4096]}\n",checks);
}
