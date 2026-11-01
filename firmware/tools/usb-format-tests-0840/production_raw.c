// Tab5OS: explicit whole-device USB formatting, independent of the old FS.

typedef struct {
 msc_host_device_handle_t device;
 uint32_t sectors,sector_size;
 bool (*present)(void*);
 void* context;
} format_disk_t;
// Exactly one explicit format task exists; its temporary drive is never
// published through VFS and never shares the mounted USB diskio callbacks.
static format_disk_t* active;
static bool available(void){return active&&active->present(active->context);}
static DSTATUS initialize(BYTE pdrv){(void)pdrv;return available()?0:STA_NODISK;}
static DSTATUS status(BYTE pdrv){return initialize(pdrv);}
static DRESULT read_sector(BYTE pdrv,BYTE* data,DWORD sector,UINT count){
 (void)pdrv;
 if(!available()||!count||sector>=active->sectors||count>active->sectors-sector)return RES_NOTRDY;
 return scsi_cmd_read10(active->device,data,sector,count,active->sector_size)==ESP_OK?RES_OK:RES_ERROR;
}
static DRESULT write_sector(BYTE pdrv,const BYTE* data,DWORD sector,UINT count){
 (void)pdrv;
 if(!available()||!count||sector>=active->sectors||count>active->sectors-sector)return RES_NOTRDY;
 return scsi_cmd_write10(active->device,data,sector,count,active->sector_size)==ESP_OK?RES_OK:RES_ERROR;
}
static DRESULT ioctl(BYTE pdrv,BYTE command,void* out){
 (void)pdrv;if(!available())return RES_NOTRDY;
 switch(command){
  case CTRL_SYNC:{esp_err_t e=tab5_scsi_sync_cache(active->device);return e==ESP_OK||e==ESP_ERR_NOT_SUPPORTED?RES_OK:RES_ERROR;}
  case GET_SECTOR_COUNT:*(DWORD*)out=active->sectors;return RES_OK;
  case GET_SECTOR_SIZE:*(WORD*)out=active->sector_size;return RES_OK;
  case GET_BLOCK_SIZE:*(DWORD*)out=1;return RES_OK;
  default:return RES_PARERR;
 }
}
esp_err_t tab5_msc_format_raw(msc_host_device_handle_t device,bool fat32,bool (*present)(void*),void* context){
 if(!device||!present||active)return ESP_ERR_INVALID_ARG;
 msc_device_t* dev=(msc_device_t*)device;
 if(dev->disk.block_size<FF_MIN_SS||dev->disk.block_size>FF_MAX_SS||
    dev->disk.block_size>4096||!dev->disk.block_size||
    (dev->disk.block_size&(dev->disk.block_size-1))||dev->disk.block_count<256)return ESP_ERR_NOT_SUPPORTED;
 const uint64_t bytes=(uint64_t)dev->disk.block_count*dev->disk.block_size;
 if(fat32?(bytes<64ULL*1024*1024||bytes>2ULL*1024*1024*1024*1024):
          (bytes<1024*1024||bytes>2ULL*1024*1024*1024))return ESP_ERR_NOT_SUPPORTED;
 BYTE pdrv;if(ff_diskio_get_drive(&pdrv)!=ESP_OK)return ESP_ERR_NO_MEM;
 // FatFs consumes ordinary memory; the MSC layer copies into its own DMA
 // transfer. Keep the bounded work buffer out of scarce internal SRAM.
 BYTE* work=heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 FATFS* fs=heap_caps_calloc(1,sizeof(FATFS),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!work||!fs){heap_caps_free(work);heap_caps_free(fs);return ESP_ERR_NO_MEM;}
 format_disk_t disk={device,dev->disk.block_count,dev->disk.block_size,present,context};
 active=&disk;
 const ff_diskio_impl_t callbacks={.init=initialize,.status=status,.read=read_sector,.write=write_sector,.ioctl=ioctl};
 ff_diskio_register(pdrv,&callbacks);
 char drive[4]={(char)('0'+pdrv),':',0,0};
 #if FF_MULTI_PARTITION
 const PARTITION old=VolToPart[pdrv];VolToPart[pdrv].pd=pdrv;VolToPart[pdrv].pt=0;
 #endif
 esp_err_t result=ESP_ERR_MSC_FORMAT_FAILED;
 // Remove stale GPT primary/backup headers before making one MBR partition.
 // No scan of the old FS, no full-disk erase and no device-sized allocation.
 memset(work,0,4096);
 const UINT block=4096/disk.sector_size;
 bool ok=true;
 for(unsigned side=0;side<2&&ok;++side){
  const DWORD start=side?disk.sectors-33:0;const UINT total=side?33:34;
  for(UINT done=0;done<total;){UINT n=total-done;if(n>block)n=block;
   if(write_sector(pdrv,work,start+done,n)!=RES_OK){ok=false;break;}done+=n;
  }
 }
 if(ok){
  const MKFS_PARM options={.fmt=fat32?FM_FAT32:FM_FAT,.n_fat=2,.align=0,.n_root=0,.au_size=0};
  FRESULT formatted=f_mkfs(drive,&options,work,4096);
  if(formatted==FR_OK&&ioctl(pdrv,CTRL_SYNC,NULL)==RES_OK&&f_mount(fs,drive,1)==FR_OK){
   const bool type=fat32?fs->fs_type==FS_FAT32:(fs->fs_type==FS_FAT12||fs->fs_type==FS_FAT16);
   if(type&&available())result=ESP_OK;
  }
 }
 f_mount(NULL,drive,0);ff_diskio_unregister(pdrv);
 #if FF_MULTI_PARTITION
 VolToPart[pdrv]=old;
 #endif
 active=NULL;heap_caps_free(fs);heap_caps_free(work);return result;
}
