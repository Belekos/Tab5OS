#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
typedef int esp_err_t;
typedef uint8_t BYTE,DSTATUS;typedef uint16_t WORD;typedef uint32_t DWORD,UINT;typedef int DRESULT,FRESULT;
typedef struct {uint32_t block_size,block_count;} usb_disk_t;
typedef struct {usb_disk_t disk;} msc_device_t;typedef msc_device_t* msc_host_device_handle_t;
typedef struct {BYTE fs_type;} FATFS;
typedef struct {BYTE fmt,n_fat;UINT align,n_root;DWORD au_size;} MKFS_PARM;
typedef struct {BYTE pd,pt;} PARTITION;
typedef struct {DSTATUS(*init)(BYTE);DSTATUS(*status)(BYTE);DRESULT(*read)(BYTE,BYTE*,DWORD,UINT);DRESULT(*write)(BYTE,const BYTE*,DWORD,UINT);DRESULT(*ioctl)(BYTE,BYTE,void*);} ff_diskio_impl_t;
enum {ESP_OK,ESP_ERR_INVALID_ARG,ESP_ERR_NOT_SUPPORTED,ESP_ERR_NO_MEM,ESP_ERR_MSC_FORMAT_FAILED,ESP_FAIL};
enum {RES_OK,RES_ERROR,RES_NOTRDY,RES_PARERR,STA_NODISK};
enum {CTRL_SYNC,GET_SECTOR_COUNT,GET_SECTOR_SIZE,GET_BLOCK_SIZE};
enum {FM_FAT=1,FM_FAT32=2,FS_FAT12=1,FS_FAT16=2,FS_FAT32=3,FR_OK=0};
#define FF_MIN_SS 512
#define FF_MAX_SS 4096
#define FF_MULTI_PARTITION 1
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static unsigned checks;
#define CHECK(x) do{++checks;assert(x);}while(0)
static PARTITION VolToPart[4]={{0,0},{1,0},{3,2},{3,0}};
static ff_diskio_impl_t callbacks;
static int allocations,live,failAllocAt,registered,unregistered,writes,mkfsCalls,mountCalls,unmounts,readCalls,failWriteAt,failMkfs,failSync,failMount,wrongType,absentAt;
static bool presentNow=true,wantedFat32;
static uint64_t erased;
static msc_device_t disk={{512,131072}};
static esp_err_t ff_diskio_get_drive(BYTE* p){*p=2;return ESP_OK;}
static void ff_diskio_register(BYTE p,const ff_diskio_impl_t* c){CHECK(p==2);if(c){callbacks=*c;++registered;}else ++unregistered;}
#define ff_diskio_unregister(p) ff_diskio_register(p,NULL)
static void* heap_caps_malloc(size_t n,unsigned caps){CHECK(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++allocations;if(allocations==failAllocAt)return NULL;void* p=malloc(n);if(p)++live;return p;}
static void* heap_caps_calloc(size_t n,size_t z,unsigned caps){void* p=heap_caps_malloc(n*z,caps);if(p)memset(p,0,n*z);return p;}
static void heap_caps_free(void* p){if(p){--live;free(p);}}
static bool present_check(void* ctx){CHECK(ctx==&disk);return presentNow;}
static esp_err_t scsi_cmd_read10(msc_host_device_handle_t d,BYTE* out,DWORD sector,UINT n,uint32_t size){(void)out;CHECK(d==&disk);CHECK(sector+n<=disk.disk.block_count);CHECK(size==disk.disk.block_size);++readCalls;return ESP_OK;}
static esp_err_t scsi_cmd_write10(msc_host_device_handle_t d,const BYTE* data,DWORD sector,UINT n,uint32_t size){
 CHECK(d==&disk);CHECK(sector+n<=disk.disk.block_count);CHECK(size==disk.disk.block_size);CHECK(n*size<=4096);++writes;
 if(mkfsCalls==0){CHECK(sector<34||sector>=disk.disk.block_count-33);for(unsigned i=0;i<n*size;++i)CHECK(data[i]==0);erased+=n;}
 if(absentAt==writes)presentNow=false;
 return failWriteAt==writes?ESP_FAIL:ESP_OK;
}
static esp_err_t tab5_scsi_sync_cache(msc_host_device_handle_t d){CHECK(d==&disk);return failSync?ESP_FAIL:ESP_OK;}
static FRESULT f_mkfs(const char* path,const MKFS_PARM* p,void* work,UINT bytes){
 ++mkfsCalls;CHECK(!strcmp(path,"2:"));CHECK(work&&bytes==4096);CHECK(p->fmt==(wantedFat32?FM_FAT32:FM_FAT));CHECK(p->n_fat==2);CHECK(p->au_size==0);CHECK(VolToPart[2].pd==2&&VolToPart[2].pt==0);
 DWORD count,block;WORD size;CHECK(callbacks.init(2)==0);CHECK(callbacks.ioctl(2,GET_SECTOR_COUNT,&count)==RES_OK&&count==disk.disk.block_count);CHECK(callbacks.ioctl(2,GET_SECTOR_SIZE,&size)==RES_OK&&size==disk.disk.block_size);CHECK(callbacks.ioctl(2,GET_BLOCK_SIZE,&block)==RES_OK&&block==1);
 return failMkfs?1:FR_OK;
}
static FRESULT f_mount(FATFS* fs,const char* path,int now){CHECK(!strcmp(path,"2:"));if(!fs){++unmounts;CHECK(!now);return FR_OK;}++mountCalls;CHECK(now==1);fs->fs_type=wrongType?99:(wantedFat32?FS_FAT32:FS_FAT16);return failMount?1:FR_OK;}
#include "raw.c"
static void reset(void){CHECK(!active);CHECK(live==0);allocations=failAllocAt=registered=unregistered=writes=mkfsCalls=mountCalls=unmounts=readCalls=failWriteAt=failMkfs=failSync=failMount=wrongType=absentAt=0;presentNow=true;wantedFat32=true;erased=0;disk.disk.block_size=512;disk.disk.block_count=131072;}
static int format(void){return tab5_msc_format_raw(&disk,wantedFat32,present_check,&disk);}
static void cleaned(void){CHECK(live==0);CHECK(active==NULL);CHECK(registered==unregistered);CHECK(VolToPart[2].pd==3&&VolToPart[2].pt==2);}
int main(void){
 reset();CHECK(format()==ESP_OK);CHECK(erased==67);CHECK(mkfsCalls==1);CHECK(mountCalls==1);CHECK(readCalls==0);cleaned();
 reset();wantedFat32=false;CHECK(format()==ESP_OK);cleaned();
 for(unsigned ss=512;ss<=4096;ss*=2){reset();disk.disk.block_size=ss;CHECK(format()==ESP_OK);CHECK(erased==67);cleaned();}
 for(int a=1;a<=2;++a){reset();failAllocAt=a;CHECK(format()==ESP_ERR_NO_MEM);CHECK(writes==0);CHECK(mkfsCalls==0);cleaned();}
 reset();presentNow=false;CHECK(format()!=ESP_OK);CHECK(writes==0);CHECK(mkfsCalls==0);cleaned();
 reset();absentAt=2;CHECK(format()!=ESP_OK);CHECK(writes==2);CHECK(mkfsCalls==0);cleaned();
 reset();failWriteAt=3;CHECK(format()!=ESP_OK);CHECK(writes==3);CHECK(mkfsCalls==0);cleaned();
 reset();failMkfs=1;CHECK(format()!=ESP_OK);CHECK(mountCalls==0);cleaned();
 reset();failSync=1;CHECK(format()!=ESP_OK);CHECK(mountCalls==0);cleaned();
 reset();failMount=1;CHECK(format()!=ESP_OK);CHECK(mountCalls==1);cleaned();
 reset();wrongType=1;CHECK(format()!=ESP_OK);cleaned();
 reset();disk.disk.block_count=2048;CHECK(format()==ESP_ERR_NOT_SUPPORTED);CHECK(writes==0);wantedFat32=false;CHECK(format()==ESP_OK);cleaned();
 reset();wantedFat32=false;disk.disk.block_count=8U*1024*1024;CHECK(format()==ESP_ERR_NOT_SUPPORTED);CHECK(allocations==0);cleaned();
 reset();disk.disk.block_size=513;CHECK(format()==ESP_ERR_NOT_SUPPORTED);CHECK(allocations==0);cleaned();
 printf("{\"passed\":true,\"checks\":%u}\n",checks);return 0;
}
