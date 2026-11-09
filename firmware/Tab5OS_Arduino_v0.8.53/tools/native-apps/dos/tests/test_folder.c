/* Executes production platform directory callbacks and the FAT projection.
 * The SDK and stdio are bounded in-memory doubles with injected failures. */
#include "Tab5AppApi.h"
#include "pc_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct dirent {char d_name[256];};
typedef struct dos_directory {int handle,failed;char path[256];struct dirent entry;} DIR;
struct test_stat {uint32_t st_mode,st_size;time_t st_mtime;};
#define S_IFDIR 0040000
#define S_IFREG 0100000
#undef S_ISDIR
#undef S_ISREG
#define S_ISDIR(m) (((m)&S_IFDIR)!=0)
#define S_ISREG(m) (((m)&S_IFREG)!=0)
#define stat test_stat
struct node {char path[512];uint32_t bytes;int directory;};
static struct node nodes[1300];static unsigned node_count;
struct handle {int node;unsigned cursor;int open;};
static struct handle handles[8];
static int stopped,fail_directory,fail_read,fail_seek,fail_open;
static unsigned stdio_reads,stdio_seeks;static uint32_t file_position;static int patterned;
static unsigned open_handles;
static char last_path[512];static struct test_stat last_stat;
char dos_mount_root[256];
static int open_directory(const char* path){
 if(fail_directory==1)return -1;
 for(unsigned n=0;n<node_count;n++)if(nodes[n].directory&&!strcmp(nodes[n].path,path)){
  for(int h=0;h<8;h++)if(!handles[h].open){handles[h]=(struct handle){(int)n,0,1};++open_handles;return h+1;}
 }
 return -1;
}
static int next_entry(int handle,struct tab5_app_directory_entry* e){
 if(fail_directory==2)return -1;
 struct handle* h=&handles[handle-1];const char* parent=nodes[h->node].path;size_t n=strlen(parent);
 while(h->cursor<node_count){struct node* node=&nodes[h->cursor++];
  if(strncmp(parent,node->path,n))continue;
  const char* name=node->path+n;if(n>1){if(*name!='/')continue;++name;}
  if(!*name||strchr(name,'/'))continue;
  strcpy(e->name,name);e->flags=node->directory?1:0;e->file_size=node->bytes;return 1;
 }
 return 0;
}
static int close_handle(int handle){assert(handle>0&&handle<=8&&handles[handle-1].open);handles[handle-1].open=0;--open_handles;return 0;}
static int stop_requested(void){return stopped;}
static int open_read(const char* path){(void)path;return -1;}
static uint32_t file_size(int handle){(void)handle;return 0;}
static struct tab5_app_api2 api2;
const struct tab5_app_api2* tab5_api=&api2;
static struct tab5_app_api7 api7;
const struct tab5_app_api7* dos_api=&api7;
// Generated from platform.c, never a reimplementation of its callbacks.
#include "directory_callbacks.inc"
static FILE* folder_file;
static FILE* fake_fopen(const char* path,const char* mode){
 (void)mode;if(fail_open)return NULL;
 for(unsigned i=0;i<node_count;++i)if(!nodes[i].directory&&!strcmp(path,nodes[i].path)){
  folder_file=(FILE*)&nodes[i];return folder_file;
 }
 return NULL;
}
static int fake_fseek(FILE* file,long offset,int whence){(void)file;assert(whence==SEEK_SET&&offset>=0);stdio_seeks++;if(fail_seek)return -1;file_position=(uint32_t)offset;return 0;}
static uint8_t pattern(const struct node* n,uint32_t at){return (uint8_t)(at*37u+(at>>8)+(unsigned)(n-nodes)*11u);}
static size_t fake_fread(void* out,size_t size,size_t count,FILE* file){
 struct node* n=(struct node*)file;stdio_reads++;assert(file_position<=n->bytes&&size*count<=n->bytes-file_position);
 if(fail_read==1)return 0;if(fail_read==2){memset(out,0xdd,size*count/2);return count/2;}
 if(patterned){for(size_t i=0;i<size*count;i++)((uint8_t*)out)[i]=pattern(n,file_position+(uint32_t)i);}else memset(out,0x5a,size*count);
 file_position+=(uint32_t)(size*count);return count;
}
static int fake_fclose(FILE* file){(void)file;folder_file=NULL;return 0;}
#define fopen fake_fopen
#define fseek fake_fseek
#define fread fake_fread
#define fclose fake_fclose
void disk_make_mbr(uint8_t* dst,uint32_t sectors,uint8_t type){(void)sectors;(void)type;memset(dst,0,512);}
void disk_make_boot(uint8_t* dst,uint32_t sectors,uint8_t per_cluster,uint16_t fat,const char* label){(void)sectors;(void)per_cluster;(void)fat;(void)label;memset(dst,0,512);}
// Complete production FAT projection, including its private bounds helpers.
#include "folder_source.inc"
static void node(const char* path,int directory,uint32_t bytes){assert(node_count<1300&&strlen(path)<512);struct node* n=&nodes[node_count++];strcpy(n->path,path);n->directory=directory;n->bytes=bytes;}
static void reset(void){assert(!open_handles);node_count=0;stopped=fail_directory=fail_read=fail_seek=fail_open=patterned=0;stdio_reads=stdio_seeks=0;file_position=0;memset(last_path,0,sizeof(last_path));strcpy(dos_mount_root,"/DOS");node("/DOS",1,0);}
static struct folder* mount(void){disk_t d={0};return folder_open(dos_mount_root,&d);}
static void expect_failed(void){assert(mount()==NULL);assert(!open_handles);}
int main(void){
 api2.base.close=close_handle;api2.base.should_stop=stop_requested;api2.base.open_read=open_read;api2.file_size=file_size;
 api7.directory_open=open_directory;api7.directory_next=next_entry;
 reset();node("/DOS/PLAY.EXE",0,24);node("/DOS/.hidden",0,1);node("/DOS/sub",1,0);node("/DOS/sub/DATA.DAT",0,520);
 struct folder* fd=mount();assert(fd&&fd->count==4&&fd->root_count==3&&!open_handles);
 char alias[13];assert(dos_folder_name(fd,"PLAY.EXE",alias)&&!strcmp(alias,"PLAY.EXE"));
 uint8_t bytes[1024];uint32_t lba=fd->data_lba+(fd->e[0].first-2)*fd->spc;
 assert(folder_read(fd,lba,1,bytes)&&bytes[0]==0x5a&&bytes[23]==0x5a&&bytes[24]==0);
 folder_close(fd);fd=mount();fail_seek=1;assert(!folder_read(fd,lba,1,bytes));folder_close(fd);
 fail_seek=0;fd=mount();fail_read=1;assert(!folder_read(fd,lba,1,bytes));folder_close(fd);
 fail_read=0;fd=mount();fail_open=1;assert(fd&&!folder_read(fd,lba,1,bytes));folder_close(fd);
 reset();node("/DOS/BIG.BIN",0,33000);node("/DOS/NEXT.BIN",0,700);patterned=1;fd=mount();assert(fd);
 lba=fd->data_lba+(fd->e[0].first-2)*fd->spc;
 for(unsigned sector=0;sector<32;sector++){assert(folder_read(fd,lba+sector,1,bytes));for(unsigned k=0;k<512;k++)assert(bytes[k]==pattern(&nodes[1],sector*512+k));}
 assert(stdio_reads==1&&stdio_seeks==1);assert(folder_read(fd,lba+32,1,bytes)&&stdio_reads==2);
 assert(folder_read(fd,lba+64,1,bytes)&&stdio_reads==3);for(unsigned k=0;k<512;k++)assert(bytes[k]==(k<232?pattern(&nodes[1],32768+k):0));
 uint32_t next_lba=fd->data_lba+(fd->e[1].first-2)*fd->spc;
 assert(folder_read(fd,next_lba,1,bytes)&&stdio_reads==4);for(unsigned k=0;k<512;k++)assert(bytes[k]==pattern(&nodes[2],k));
 assert(folder_read(fd,lba,1,bytes)&&stdio_reads==5);for(unsigned k=0;k<512;k++)assert(bytes[k]==pattern(&nodes[1],k));
 fail_read=2;assert(!folder_read(fd,lba+32,1,bytes));fail_read=0;assert(folder_read(fd,lba,1,bytes)&&stdio_reads==7);
 for(unsigned k=0;k<512;k++)assert(bytes[k]==pattern(&nodes[1],k));
 fail_seek=1;assert(!folder_read(fd,lba+32,1,bytes));fail_seek=0;assert(folder_read(fd,lba+32,1,bytes));
 for(unsigned k=0;k<512;k++)assert(bytes[k]==pattern(&nodes[1],16384+k));folder_close(fd);
 reset();node("/DOS/\xc3\xa9.COM",0,1);node("/DOS/\xc3\xa8.COM",0,1);node("/DOS/%PATH%.EXE",0,1);node("/DOS/A&B.EXE",0,1);
 fd=mount();assert(fd&&fd->count==4);
 assert(dos_folder_name(fd,"\xc3\xa9.COM",alias)&&!strcmp(alias,"__~1.COM"));
 assert(dos_folder_name(fd,"\xc3\xa8.COM",alias)&&!strcmp(alias,"__~2.COM"));
 assert(dos_folder_name(fd,"%PATH%.EXE",alias)&&!strcmp(alias,"_PATH_~1.EXE"));
 assert(dos_folder_name(fd,"A&B.EXE",alias)&&!strcmp(alias,"A_B~1.EXE"));
 lba=fd->data_lba+(fd->e[0].first-2)*fd->spc;assert(folder_read(fd,lba,1,bytes)&&bytes[0]==0x5a);folder_close(fd);
 reset();fail_directory=1;expect_failed();reset();fail_directory=2;expect_failed();reset();stopped=1;expect_failed();
 reset();for(unsigned i=0;i<511;i++){char path[64];snprintf(path,sizeof(path),"/DOS/F%u.COM",i);node(path,0,1);}
 fd=mount();assert(fd&&fd->root_count==511);folder_close(fd);node("/DOS/OVER.COM",0,1);expect_failed();
 reset();node("/DOS/A",1,0);node("/DOS/B",1,0);
 for(unsigned i=0;i<1022;i++){char path[64];snprintf(path,sizeof(path),"/DOS/%c/F%u.COM",i<511?'A':'B',i);node(path,0,1);}
 fd=mount();assert(fd&&fd->count==1024);folder_close(fd);node("/DOS/B/OVER.COM",0,1);expect_failed();
 reset();char deep[256]="/DOS";for(unsigned i=0;i<16;i++){strcat(deep,"/x");node(deep,1,0);}
 fd=mount();assert(fd);folder_close(fd);strcat(deep,"/x");node(deep,1,0);expect_failed();
 reset();node("/DOS/HUGE.DAT",0,UINT32_MAX);expect_failed();
 reset();char path[512]="/DOS/";memset(path+5,'x',246);path[251]=0;node(path,1,0);strcat(path,"/X.COM");node(path,0,1);expect_failed();
 reset();node_count=0;strcpy(dos_mount_root,"/");node("/",1,0);node("/PLAY.EXE",0,1);
 fd=mount();assert(fd&&dos_folder_name(fd,"PLAY.EXE",alias));lba=fd->data_lba+(fd->e[0].first-2)*fd->spc;assert(folder_read(fd,lba,1,bytes));folder_close(fd);
 assert(!open_handles);puts("DOS folder tests passed: SDK errors/cancellation, 511/1024/16 bounds, FAT capacity, UTF-8/shell-safe aliases, hidden/adjacent files, root paths, 16 KiB read-ahead, EOF, file changes and read-failure recovery.");return 0;
}
