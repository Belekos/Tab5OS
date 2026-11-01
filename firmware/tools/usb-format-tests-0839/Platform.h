#pragma once
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <deque>
#include <functional>
#include <cstdint>
#include "Tab5UsbStorage.h"
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_NO_MEM=1,ESP_ERR_NOT_SUPPORTED=2,ESP_FAIL=3,pdTRUE=1,pdPASS=1;
struct msc_host_event_t {enum Event{MSC_DEVICE_CONNECTED,MSC_DEVICE_DISCONNECTED}event;union{uint8_t address;void* handle;}device;};
using msc_host_device_handle_t=void*;using msc_host_vfs_handle_t=void*;
struct msc_host_device_info_t {uint32_t sector_count,sector_size;};
struct msc_host_driver_config_t {bool create_backround_task;int task_priority,stack_size,core_id;void(*callback)(const msc_host_event_t*,void*);};
struct esp_vfs_fat_mount_config_t {bool format_if_mount_failed;int max_files;size_t allocation_unit_size;};
using QueueHandle_t=std::deque<msc_host_event_t>*;
static QueueHandle_t queue;
static void(*eventCallback)(const msc_host_event_t*,void*);
static void(*queuedTask)(void*);
static bool failTask=false,failMount=false,exclusiveAvailable=true,sdConflict=false,failInstall=false,failFormat=false;
static int rawCalls=0,mountCalls=0,unmountCalls=0,uninstallCalls=0,syncError=ESP_OK,delayCalls=0;
static uint32_t sectors=131072,sectorSize=512;
static bool lastFat32=false;
static std::function<void()> onDelay,onRaw;
static void* handle=reinterpret_cast<void*>(uintptr_t(0x1234));
struct SDMock{bool exists(const char*){return sdConflict;}};static SDMock SD;
class Tab5UsbStorageRecovery {bool held;public:Tab5UsbStorageRecovery():held(exclusiveAvailable){}explicit operator bool()const{return held;}};
static QueueHandle_t xQueueCreate(unsigned,size_t){queue=new std::deque<msc_host_event_t>;return queue;}
static void vQueueDelete(QueueHandle_t q){delete q;}
static int xQueueSend(QueueHandle_t q,const msc_host_event_t* e,int){q->push_back(*e);return pdTRUE;}
static int xQueueReceive(QueueHandle_t q,msc_host_event_t* e,int){if(q->empty())return 0;*e=q->front();q->pop_front();return pdTRUE;}
static int msc_host_install(msc_host_driver_config_t* c){eventCallback=c->callback;return ESP_OK;}
static int msc_host_install_device(uint8_t,void** out){if(failInstall)return ESP_FAIL;*out=handle;return ESP_OK;}
static int msc_host_get_device_info(void*,msc_host_device_info_t* i){i->sector_count=sectors;i->sector_size=sectorSize;return ESP_OK;}
static int msc_host_vfs_register(void*,const char*,esp_vfs_fat_mount_config_t* c,void** out){assert(!c->format_if_mount_failed);++mountCalls;if(failMount)return ESP_FAIL;*out=reinterpret_cast<void*>(uintptr_t(0x5678));return ESP_OK;}
static int msc_host_vfs_unregister(void*){++unmountCalls;return ESP_OK;}
static int msc_host_uninstall_device(void*){++uninstallCalls;return ESP_OK;}
static int tab5_scsi_sync_cache(void*){return syncError;}
static int tab5_msc_format_raw(void*,bool fat32,bool(*present)(void*),void* arg){assert(present(arg));++rawCalls;lastFat32=fat32;if(onRaw)onRaw();failMount=false;return failFormat?ESP_FAIL:ESP_OK;}
static int tab5CreateTask(void(*fn)(void*),const char*,int,void*,int,void*,int){if(failTask)return 0;assert(!queuedTask);queuedTask=fn;return pdPASS;}
static unsigned pdMS_TO_TICKS(unsigned n){return n;}
static void vTaskDelay(unsigned){++delayCalls;assert(delayCalls<20);if(onDelay)onDelay();}
static void vTaskDelete(void*){}
static void tab5TaskCheckpoint(const char*){}
