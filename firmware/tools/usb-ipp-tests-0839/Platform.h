#pragma once
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include "Tab5UsbPrinter.h"
#include "Tab5UsbIppProtocol.h"
using esp_err_t=int;using usb_host_client_handle_t=void*;using usb_device_handle_t=void*;
constexpr int ESP_OK=0,ESP_FAIL=1,pdTRUE=1,pdPASS=1,USB_HOST_CLIENT_EVENT_NEW_DEV=1,USB_HOST_CLIENT_EVENT_DEV_GONE=2,USB_TRANSFER_STATUS_COMPLETED=0;
constexpr unsigned portMAX_DELAY=~0U,MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
struct usb_transfer_t{uint8_t* data_buffer=nullptr;usb_device_handle_t device_handle;uint8_t bEndpointAddress;int num_bytes,actual_num_bytes=0,status=0;void(*callback)(usb_transfer_t*);void* context;uint32_t timeout_ms;};
struct usb_host_client_event_msg_t{int event;struct{uint8_t address;}new_dev;struct{void* dev_hdl;}dev_gone;};
struct usb_host_client_config_t{bool is_synchronous;int max_num_event_msg;struct{void(*client_event_callback)(const usb_host_client_event_msg_t*,void*);void*callback_arg;}async;};
#pragma pack(push,1)
struct usb_config_desc_t{uint8_t bLength,bDescriptorType;uint16_t wTotalLength;uint8_t rest[28];};
#pragma pack(pop)
struct Sem{int count;};using SemaphoreHandle_t=Sem*;using QueueHandle_t=std::deque<uint8_t>*;
static bool reserveFail=false,allocateFail=false,taskFail=false,claimFail=false,shortOut=false,timeoutInput=false;
static bool pendingTransfer=false;
static int allocations=0,live=0,submitted=0,outs=0,ins=0,claims=0,releases=0,closed=0,halts=0,flushes=0;
static uint32_t now=0;static size_t fragment=2048,rxAt=0;
static std::string tx,rx;static std::function<void()> onSubmit;
static void(*queuedTask)(void*);static void(*usbEvent)(const usb_host_client_event_msg_t*,void*);
static usb_transfer_t* lastTransfer=nullptr;
static uint8_t descriptor[]={9,2,32,0,1,1,0,0x80,50,9,4,0,0,2,7,1,4,0,7,5,0x81,2,0,2,0,7,5,0x02,2,0,2,0};
static void* usbDevice=reinterpret_cast<void*>(uintptr_t(0x1234));
static SemaphoreHandle_t xSemaphoreCreateMutex(){return new Sem{1};}
static SemaphoreHandle_t xSemaphoreCreateBinary(){return new Sem{0};}
static void vSemaphoreDelete(Sem* s){delete s;}
static int xSemaphoreTake(Sem* s,unsigned){if(s&&s->count){--s->count;return pdTRUE;}return 0;}
static void xSemaphoreGive(Sem* s){++s->count;}
static QueueHandle_t xQueueCreate(unsigned,size_t){return new std::deque<uint8_t>;}
static void vQueueDelete(QueueHandle_t q){delete q;}
static int xQueueSend(QueueHandle_t q,const uint8_t* b,int){q->push_back(*b);return pdTRUE;}
static int xQueueReceive(QueueHandle_t q,uint8_t* b,int){if(q->empty())return 0;*b=q->front();q->pop_front();return pdTRUE;}
static unsigned pdMS_TO_TICKS(unsigned n){return n;}
static unsigned millis(){return now;}
static void vTaskDelay(unsigned n){now+=n;}
static int usb_host_client_register(usb_host_client_config_t* c,void** p){usbEvent=c->async.client_event_callback;*p=reinterpret_cast<void*>(1);return ESP_OK;}
static int usb_host_client_deregister(void*){return ESP_OK;}
static int usb_host_client_handle_events(void*,unsigned){return ESP_OK;}
static int usb_host_client_unblock(void*){return ESP_OK;}
static int usb_host_device_open(void*,uint8_t,void** p){*p=usbDevice;return ESP_OK;}
static int usb_host_device_close(void*,void*){++closed;return ESP_OK;}
static int usb_host_get_active_config_descriptor(void*,const usb_config_desc_t** p){*p=reinterpret_cast<const usb_config_desc_t*>(descriptor);return ESP_OK;}
static int usb_host_interface_claim(void*,void*,uint8_t,uint8_t){++claims;return claimFail?ESP_FAIL:ESP_OK;}
static int usb_host_interface_release(void*,void*,uint8_t){++releases;return ESP_OK;}
static int usb_host_transfer_alloc(size_t n,int,usb_transfer_t** p){*p=new usb_transfer_t;(*p)->data_buffer=new uint8_t[n];return ESP_OK;}
static int usb_host_transfer_free(usb_transfer_t* t){assert(!pendingTransfer);delete[] t->data_buffer;delete t;return ESP_OK;}
static int usb_host_transfer_submit(usb_transfer_t* t){
 ++submitted;lastTransfer=t;t->status=USB_TRANSFER_STATUS_COMPLETED;
 if(t->bEndpointAddress&0x80){++ins;const size_t n=std::min({size_t(t->num_bytes),fragment,rx.size()-rxAt});memcpy(t->data_buffer,rx.data()+rxAt,n);rxAt+=n;t->actual_num_bytes=int(n);if(!n)now+=1001;if(timeoutInput){pendingTransfer=true;return ESP_OK;}}
 else{++outs;t->actual_num_bytes=shortOut?t->num_bytes/2:t->num_bytes;tx.append(reinterpret_cast<char*>(t->data_buffer),size_t(t->actual_num_bytes));}
 if(onSubmit)onSubmit();t->callback(t);return ESP_OK;
}
static int usb_host_endpoint_halt(void*,uint8_t){++halts;return ESP_OK;}
static int usb_host_endpoint_flush(void*,uint8_t){++flushes;if(pendingTransfer){pendingTransfer=false;lastTransfer->callback(lastTransfer);}return ESP_OK;}
static int usb_host_endpoint_clear(void*,uint8_t){return ESP_OK;}
static void* heap_caps_malloc(size_t n,unsigned caps){assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++allocations;if(allocateFail)return nullptr;void* p=malloc(n);if(p)++live;return p;}
static void heap_caps_free(void* p){if(p){--live;free(p);}}
namespace Tab5Resources{enum class Owner{Tools};struct AdmissionReport{};struct Budget{size_t a,b,c,d,e;};struct Reservation{bool held;explicit operator bool()const{return held;}void release(){held=false;}};}
static Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Budget,const char*,Tab5Resources::AdmissionReport*){return {!reserveFail};}
static int tab5CreateTask(void(*f)(void*),const char*,int,void*,int,void*,int){if(taskFail)return 0;queuedTask=f;return pdPASS;}
