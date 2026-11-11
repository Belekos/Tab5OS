/* Reuse the bounded FAT16 directory projection and floppy COW implementation.
 * Device handles are a fixed index, independent of OS file descriptors. */
#include "storage.h"
#include "../dos/platform.h"
#include "../common/runtime.h"
#include "../dos/core/ibmpc/pc_internal.h"
#include <stdio.h>
#include <string.h>
disk_t disks[PC_FLOPPIES+PC_HARD_DISKS];
static int handles[2]={-1,-1};
static uint32_t identity,io_ms;
static const char *const paths[]={"/__dos386/fd0","/__dos386/fd1","/__dos386/hd0"};
static int valid(int d){return d>=0&&d<3&&disks[d].present;}
void x86_storage_close(void){
 for(int i=0;i<2;i++){if(handles[i]>=0)tab5_api->base.close(handles[i]);handles[i]=-1;}
 if(disks[2].folder)folder_close(disks[2].folder);
 memset(disks,0,sizeof(disks));
}
static int floppy(int d,const char *path){
 int h=tab5_api->base.open_read(path);if(h<0)return -1;
 uint32_t size=tab5_api->file_size(h);
 if(size!=1440u*1024u){tab5_api->base.close(h);return -1;}
 handles[d]=h;disks[d].present=true;disks[d].readonly=d!=0;
 disks[d].total=size/512;disks[d].cylinders=80;disks[d].heads=2;disks[d].sectors=18;return 0;
}
int x86_storage_open(const char *folder,const char *argument,char program[13]){
 x86_storage_close();identity=0;io_ms=0;program[0]=0;
 if(!folder||strlen(folder)>=sizeof(dos_mount_root))return -1;
 if(folder!=dos_mount_root)memmove(dos_mount_root,folder,strlen(folder)+1);
 if(floppy(0,"/DOS/FREEDOS.IMG"))return -1;
 (void)floppy(1,"/System/DOS/DRIVERS.IMG");
 disks[2].folder=folder_open(folder,&disks[2]);if(!disks[2].folder)goto fail;
 disks[2].present=true;disks[2].readonly=true;
 if(argument&&*argument){const char *name=strrchr(argument,'/');
  if(!name||!dos_folder_name(disks[2].folder,name+1,program))goto fail;}
 identity=tab5_crc32(0,"DOS386-COW-v1",13);
 identity=tab5_crc32(identity,folder,strlen(folder))^dos_folder_fingerprint(disks[2].folder);
 uint8_t data[4096];uint32_t left=disks[0].total*512;
 while(left){uint32_t n=left>sizeof(data)?sizeof(data):left;
  if(tab5_api->base.should_stop()||tab5_api->base.read(handles[0],data,n)!=(int)n)goto fail;
  identity=tab5_crc32(identity,data,n);left-=n;tab5_api->base.sleep_ms(0);}
 if(tab5_api->base.seek(handles[0],0))goto fail;
 return 0;
fail:x86_storage_close();return -1;
}
uint32_t x86_storage_identity(void){return identity;}
int x86_storage_has_mouse_driver(void){return valid(1);}
uint32_t x86_storage_time_ms(void){return io_ms;}
int x86_disk_open(const char *path,int readonly){
 (void)readonly;if(!path)return -1;
 for(int i=0;i<3;i++)if(!strcmp(path,paths[i])&&valid(i))return i;
 return -1;
}
uint32_t x86_disk_sectors(int d){return valid(d)?disks[d].total:0;}
int x86_disk_readonly(int d){return !valid(d)||disks[d].readonly;}
void x86_disk_close(int d){(void)d;}
void x86_disk_geometry(int d,int *c,int *h,int *s){
 if(c)*c=valid(d)?disks[d].cylinders:0;
 if(h)*h=valid(d)?disks[d].heads:0;
 if(s)*s=valid(d)?disks[d].sectors:0;
}
int x86_disk_read(int d,uint32_t lba,uint32_t count,uint8_t *bytes){
 if(!valid(d)||!bytes||lba>disks[d].total||count>disks[d].total-lba||count>UINT32_MAX/512)return -1;
 uint32_t started=tab5_api->base.millis();int result=-1;
 if(d==2)result=folder_read(disks[2].folder,lba,count,bytes)?0:-1;
 else if(!tab5_api->base.seek(handles[d],lba*512u)){
  uint32_t size=count*512u,done=0;result=0;
  while(done<size){uint32_t n=size-done;if(n>4096)n=4096;
   if(tab5_api->base.should_stop()||tab5_api->base.read(handles[d],bytes+done,n)!=(int)n){result=-1;break;}done+=n;}
  if(!result&&d==0)dos_overlay_apply(d,lba,count,bytes);
 }
 io_ms+=tab5_api->base.millis()-started;return result;
}
int x86_disk_write(int d,uint32_t lba,uint32_t count,const uint8_t *bytes){
 if(d!=0||!valid(d)||!bytes||lba>disks[d].total||count>disks[d].total-lba)return -1;
 return dos_overlay_write(d,lba,count,bytes)?0:-1;
}
