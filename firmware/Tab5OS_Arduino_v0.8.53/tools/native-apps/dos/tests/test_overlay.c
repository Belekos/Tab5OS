/* Exercise the production overlay against an atomic-save/read-only OS mock. */
#include "platform.h"
#include "../common/runtime.h"
#include "pc_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
disk_t disks[4];
static uint8_t *saved;static size_t saved_size,position;static int reject_save;
static int mock_open(const char *path){(void)path;position=0;return saved?1:-1;}
static int mock_read(int fd,void *out,size_t n){assert(fd==1);if(n>saved_size-position)n=saved_size-position;memcpy(out,saved+position,n);position+=n;return (int)n;}
static int mock_close(int fd){assert(fd==1);return 0;}
static uint32_t mock_size(int fd){assert(fd==1);return (uint32_t)saved_size;}
static int mock_save(const char *path,const void *bytes,size_t n){assert(!strncmp(path,"/Saves/DOS/",11));assert(n<=2u*1024u*1024u);if(reject_save)return -1;uint8_t *next=malloc(n);assert(next);memcpy(next,bytes,n);free(saved);saved=next;saved_size=n;return 0;}
static const struct tab5_app_api2 api={.base={.open_read=mock_open,.read=mock_read,.close=mock_close},.file_size=mock_size,.save=mock_save};
const struct tab5_app_api2 *tab5_api=&api;
uint32_t tab5_crc32(uint32_t c,const void *data,size_t n){const uint8_t *p=data;c=~c;while(n--){c^=*p++;for(int i=0;i<8;i++)c=(c>>1)^(0xedb88320u&-(int32_t)(c&1));}return ~c;}
void tab5_save_name(char *out,const char *sys,const char *ext,uint32_t crc){snprintf(out,80,"/Saves/%s/%08x.%s",sys,(unsigned)crc,ext);}
int tab5_load(int fd,void *out,size_t n){return mock_read(fd,out,n)==(int)n;}
static void repair_crc(void){uint32_t crc=tab5_crc32(0,saved+20,saved_size-20);memcpy(saved+16,&crc,4);}
int main(void){
 disks[0].present=true;disks[0].total=2880;
 dos_overlay_init(42);assert(dos_overlay_load()==0);assert(!dos_overlay_dirty());
 uint8_t original[1536],data[1024],out[1536];memset(original,0x11,sizeof(original));memset(data,0xa5,512);memset(data+512,0x5a,512);
 assert(!dos_overlay_write(2,0,1,data));assert(!dos_overlay_write(0,2880,1,data));assert(!dos_overlay_write(0,UINT32_MAX,2,data));
 assert(dos_overlay_write(0,10,2,data));memcpy(out,original,sizeof(out));dos_overlay_apply(0,9,3,out);assert(!memcmp(out,original,512));assert(!memcmp(out+512,data,1024));
 reject_save=1;assert(dos_overlay_save()==-1&&dos_overlay_dirty()&&!saved);reject_save=0;assert(!dos_overlay_save()&&!dos_overlay_dirty());assert(saved_size==20+2*520);
 uint32_t committed=tab5_crc32(0,saved,saved_size);data[0]=9;assert(dos_overlay_write(0,10,1,data));reject_save=1;assert(dos_overlay_save()==-1&&dos_overlay_dirty());assert(committed==tab5_crc32(0,saved,saved_size));reject_save=0;
 dos_overlay_init(42);assert(dos_overlay_load()==1);memset(out,0,sizeof(out));dos_overlay_apply(0,10,2,out);assert(out[0]==0xa5&&out[512]==0x5a);
 saved[saved_size-1]^=1;dos_overlay_init(42);assert(dos_overlay_load()==-1);saved[saved_size-1]^=1;
 dos_overlay_init(43);assert(dos_overlay_load()==-1);
 /* Valid CRC is insufficient: reject non-A drives, impossible LBAs, duplicate sectors. */
 saved[20]=2;repair_crc();dos_overlay_init(42);assert(dos_overlay_load()==-1);saved[20]=0;
 uint32_t invalid=2880;memcpy(saved+24,&invalid,4);repair_crc();dos_overlay_init(42);assert(dos_overlay_load()==-1);invalid=10;memcpy(saved+24,&invalid,4);
 memcpy(saved+20+520+4,&invalid,4);repair_crc();dos_overlay_init(42);assert(dos_overlay_load()==-1);
 uint32_t too_many=3073;memcpy(saved+12,&too_many,4);dos_overlay_init(42);assert(dos_overlay_load()==-1);
 /* The entire supported floppy can be changed and persisted below the SDK's 2 MiB limit. */
 dos_overlay_init(99);uint8_t *full=malloc(2880u*512u);assert(full);for(size_t i=0;i<2880u*512u;i++)full[i]=(uint8_t)i;
 assert(dos_overlay_write(0,0,2880,full));assert(!dos_overlay_save());assert(saved_size==20+2880u*520u);
 dos_overlay_init(99);assert(dos_overlay_load()==1);memset(out,0,sizeof(out));dos_overlay_apply(0,2877,3,out);assert(!memcmp(out,full+2877u*512u,sizeof(out)));
 free(full);free(saved);puts("DOS overlay checks passed: A-only, bounds, save failure, reload, CRC, identity, duplicate/count rejection, full floppy.");return 0;
}
