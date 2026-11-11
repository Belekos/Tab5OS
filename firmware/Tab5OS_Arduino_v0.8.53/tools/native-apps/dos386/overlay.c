/* Guest writes live in this bounded sector overlay. Host images and files are
 * opened read-only. Persistence uses the OS's existing atomic save callback. */
#include "../dos/platform.h"
#include "../common/runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pc_internal.h"
#define MAX_SECTORS 3072u
#define HASH_SLOTS 8192u
struct sector {uint32_t drive,lba;uint8_t bytes[512];};
struct header {uint32_t magic,version,identity,count,crc;};
static struct sector *sectors;static uint8_t *snapshot;static uint16_t *index_map;static uint32_t used,identity;static int dirty;
static char save_path[80];
static uint32_t slot_for(uint32_t drive,uint32_t lba){
 uint32_t h=(lba*2654435761u+drive*113u)&(HASH_SLOTS-1);
 while(index_map[h]){struct sector *s=&sectors[index_map[h]-1];if(s->drive==drive&&s->lba==lba)break;h=(h+1)&(HASH_SLOTS-1);}return h;
}
void dos_overlay_init(uint32_t id){identity=id;used=0;dirty=0;snapshot=calloc(1,sizeof(struct header)+MAX_SECTORS*sizeof(*sectors));sectors=(void*)(snapshot+sizeof(struct header));index_map=calloc(HASH_SLOTS,sizeof(*index_map));tab5_save_name(save_path,"DOS","state",id);}
int dos_overlay_dirty(void){return dirty;}
void dos_overlay_apply(int drive,uint32_t lba,uint32_t count,uint8_t *bytes){
 for(uint32_t i=0;i<count;i++){uint16_t n=index_map[slot_for((uint32_t)drive,lba+i)];if(n)memcpy(bytes+i*512,sectors[n-1].bytes,512);}
}
int dos_overlay_write(int drive,uint32_t lba,uint32_t count,const uint8_t *bytes){
 if(drive!=0||lba>disks[0].total||count>disks[0].total-lba)return 0;
 uint32_t need=0;for(uint32_t i=0;i<count;i++)need+=!index_map[slot_for((uint32_t)drive,lba+i)];
 if(need>MAX_SECTORS-used){puts("DOS: the disk write overlay has reached its capacity.");return 0;}
 for(uint32_t i=0;i<count;i++){uint32_t h=slot_for((uint32_t)drive,lba+i);uint16_t n=index_map[h];if(!n){n=(uint16_t)++used;index_map[h]=n;sectors[n-1].drive=(uint32_t)drive;sectors[n-1].lba=lba+i;}memcpy(sectors[n-1].bytes,bytes+i*512,512);}dirty=1;return 1;
}
int dos_overlay_save(void){
 if(!dirty)return 0;size_t bytes=sizeof(struct header)+used*sizeof(*sectors);
 struct header h={0x444f5337u,1,identity,used,tab5_crc32(0,sectors,used*sizeof(*sectors))};memcpy(snapshot,&h,sizeof(h));
 int result=tab5_api->save(save_path,snapshot,bytes);if(!result)dirty=0;return result;
}
int dos_overlay_load(void){
 int handle=tab5_api->base.open_read(save_path);if(handle<0)return 0;struct header h;int ok=tab5_api->base.read(handle,&h,sizeof(h))==sizeof(h)&&h.magic==0x444f5337u&&h.version==1&&h.identity==identity&&h.count<=MAX_SECTORS&&tab5_api->file_size(handle)==sizeof(h)+h.count*sizeof(*sectors);
 if(ok)ok=tab5_load(handle,sectors,h.count*sizeof(*sectors))&&h.crc==tab5_crc32(0,sectors,h.count*sizeof(*sectors));tab5_api->base.close(handle);
 if(!ok)return -1;
 used=h.count;for(uint32_t i=0;i<used;i++){if(sectors[i].drive!=0||!disks[0].present||sectors[i].lba>=disks[0].total){ok=0;break;}uint32_t slot=slot_for(sectors[i].drive,sectors[i].lba);if(index_map[slot]){ok=0;break;}index_map[slot]=(uint16_t)(i+1);}
 if(!ok){used=0;memset(index_map,0,HASH_SLOTS*sizeof(*index_map));return -1;}return 1;
}
