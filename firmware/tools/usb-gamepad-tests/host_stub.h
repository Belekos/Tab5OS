/* USB/RTOS boundary doubles. Driver functions are extracted verbatim below. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "src/usb_hid/input_interface.h"
typedef int esp_err_t;
typedef int BaseType_t;
typedef void *SemaphoreHandle_t, *usb_device_handle_t, *usb_host_client_handle_t, *hid_host_device_handle_t;
typedef int hid_host_interface_event_t, hid_host_driver_event_t;
typedef struct usb_transfer { void *device_handle, *context; void (*callback)(struct usb_transfer*); int timeout_ms,num_bytes,bEndpointAddress,actual_num_bytes,status; uint8_t *data_buffer; size_t data_buffer_size;} usb_transfer_t;
#pragma pack(push,1)
typedef struct {uint8_t len,type,number,alternate,endpoints,klass,subclass,protocol,index;} usb_intf_desc_t;
#define bInterfaceNumber number
#define bInterfaceSubClass subclass
#define bInterfaceProtocol protocol
typedef struct {uint8_t len,type,lo,hi,bCountryCode,count,report; uint16_t wReportDescriptorLength;} hid_descriptor_t;
typedef struct {uint8_t len,type,bEndpointAddress,bmAttributes;uint16_t wMaxPacketSize;uint8_t interval;} usb_ep_desc_t;
typedef struct {uint8_t len,type;uint16_t wTotalLength;uint8_t count,value,index,flags,power;} usb_config_desc_t;
typedef struct {uint16_t idVendor;} usb_device_desc_t;
typedef struct {uint8_t bmRequestType,bRequest;uint16_t wValue,wIndex,wLength;} usb_setup_packet_t;
#pragma pack(pop)
#define USB_SETUP_PACKET_SIZE 8
typedef struct {uint8_t addr,iface_num,sub_class,proto;bool xinput;} hid_host_dev_params_t;
typedef void (*hid_host_interface_event_cb_t)(hid_host_device_handle_t,hid_host_interface_event_t,void*);
typedef void (*hid_host_driver_event_cb_t)(hid_host_device_handle_t,hid_host_driver_event_t,void*);
typedef struct {hid_host_interface_event_cb_t callback;void *callback_arg;} hid_host_device_config_t;
enum {ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NO_MEM=1,ESP_ERR_INVALID_ARG=2,ESP_ERR_INVALID_STATE=3,ESP_ERR_TIMEOUT=4,ESP_ERR_NOT_FOUND=5,ESP_ERR_INVALID_RESPONSE=6};
enum {HID_HOST_DRIVER_EVENT_CONNECTED=1,HID_HOST_INTERFACE_EVENT_DISCONNECTED=2,USB_TRANSFER_STATUS_COMPLETED=0,USB_TRANSFER_STATUS_NO_DEVICE=7,USB_TRANSFER_STATUS_CANCELED=8,HID_HOST_INTERFACE_EVENT_INPUT_REPORT=9,HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR=10,HID_SUBCLASS_BOOT_INTERFACE=1,HID_PROTOCOL_NONE=0,HID_PROTOCOL_MAX=3};
enum {HID_HOST_INTERFACE_EVENT_SUSPENDED=3,HID_HOST_INTERFACE_EVENT_RESUMED=4};
#define USB_EP_DESC_GET_MPS(e) ((e)->wMaxPacketSize & 0x7ff)
#define HID_MIN_REPORT_DESC_LEN 512u
#define DEFAULT_TIMEOUT_MS 5000
#define pdTRUE 1
#define pdMS_TO_TICKS(v) (v)
#define TAG "test"
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_EARLY_LOGE(...) ((void)0)
#define ESP_LOG_BUFFER_HEXDUMP(...) ((void)0)
#define HID_ENTER_CRITICAL() ((void)0)
#define HID_EXIT_CRITICAL() ((void)0)
#define HID_RETURN_ON_INVALID_ARG(x) do{if(!(x))return ESP_ERR_INVALID_ARG;}while(0)
#define HID_RETURN_ON_FALSE(x,e,...) do{if(!(x))return(e);}while(0)
#define HID_RETURN_ON_ERROR(x,...) do{esp_err_t err_=(x);if(err_)return err_;}while(0)
#define HID_GOTO_ON_FALSE(x,e,...) do{if(!(x)){ret=(e);goto fail;}}while(0)
#define HID_GOTO_ON_FALSE_CRITICAL HID_GOTO_ON_FALSE
#define HID_GOTO_ON_ERROR(x,...) do{ret=(x);if(ret)goto fail;}while(0)
static int fatal_calls,invalid_removals,live_allocations,alloc_attempt,fail_allocation=-1,claims,fail_submit,disconnect_events,notifications,checks,failures;
#define ESP_ERROR_CHECK(x) do{if((x)!=ESP_OK)++fatal_calls;}while(0)
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
static void *test_calloc(size_t n,size_t size){if(alloc_attempt++==fail_allocation)return NULL;void *p=calloc(n,size);if(p)++live_allocations;return p;}
static void test_free(void *p){if(p){--live_allocations;free(p);}}
#define calloc test_calloc
#define free test_free
#define STAILQ_ENTRY(t) struct {struct t *next;}
#define STAILQ_HEAD(n,t) struct n {struct t *first,*last;}
#define STAILQ_FIRST(q) ((q)->first)
#define STAILQ_NEXT(p,f) ((p)->f.next)
#define STAILQ_FOREACH(p,q,f) for((p)=(q)->first;(p);(p)=(p)->f.next)
#define STAILQ_INSERT_TAIL(q,p,f) do{if((q)->last)(q)->last->f.next=(p);else(q)->first=(p);(q)->last=(p);(p)->f.next=NULL;}while(0)
#define STAILQ_REMOVE(q,p,t,f) do{struct t **at_=&(q)->first,*prev_=NULL;while(*at_&&*at_!=(p)){prev_=*at_;at_=&(*at_)->f.next;}if(*at_){*at_=(p)->f.next;if((q)->last==(p))(q)->last=prev_;}else ++invalid_removals;}while(0)
static SemaphoreHandle_t xSemaphoreCreateBinary(void){return calloc(1,1);}
static SemaphoreHandle_t xSemaphoreCreateMutex(void){return calloc(1,1);}
static void vSemaphoreDelete(SemaphoreHandle_t p){free(p);}
static int xSemaphoreTake(SemaphoreHandle_t p,unsigned t){(void)t;return p!=NULL;}
static void xSemaphoreGive(SemaphoreHandle_t p){(void)p;}
static esp_err_t usb_host_transfer_alloc(size_t size,int z,usb_transfer_t **out){(void)z;*out=calloc(1,sizeof(**out)+size);if(!*out)return ESP_ERR_NO_MEM;(*out)->data_buffer_size=size;(*out)->data_buffer=(uint8_t*)(*out+1);return ESP_OK;}
static esp_err_t usb_host_transfer_free(usb_transfer_t *p){if(!p)return ESP_ERR_INVALID_ARG;free(p);return ESP_OK;}
static esp_err_t usb_host_interface_claim(void *c,void *d,unsigned i,unsigned a){(void)c;(void)d;(void)i;(void)a;++claims;return ESP_OK;}
static esp_err_t usb_host_interface_release(void *c,void *d,unsigned i){(void)c;(void)d;(void)i;--claims;return ESP_OK;}
static unsigned inputSubmits;
static esp_err_t usb_host_transfer_submit(usb_transfer_t *t){(void)t;++inputSubmits;return fail_submit?ESP_FAIL:ESP_OK;}
static unsigned xinput_init_calls;
static int xinput_init_stall;
static esp_err_t usb_host_transfer_submit_control(void *c,usb_transfer_t *t){
 (void)c;
 if(t->data_buffer&&t->num_bytes==28){
  usb_setup_packet_t *s=(usb_setup_packet_t*)t->data_buffer;
  if(s->bmRequestType==0xc1){CHECK(s->bRequest==1&&s->wValue==0x100&&s->wLength==20);CHECK(t->num_bytes==28);++xinput_init_calls;t->status=xinput_init_stall?1:USB_TRANSFER_STATUS_COMPLETED;t->actual_num_bytes=xinput_init_stall?8:28;}
 }
 return usb_host_transfer_submit(t);
}
static const usb_config_desc_t *test_config;
static esp_err_t usb_host_device_open(void *c,uint8_t addr,void **out){(void)c;(void)addr;*out=(void*)2;return ESP_OK;}
static esp_err_t usb_host_get_active_config_descriptor(void *d,const usb_config_desc_t **out){(void)d;*out=test_config;return ESP_OK;}
static esp_err_t usb_host_get_device_descriptor(void *d,const usb_device_desc_t **out){(void)d;static const usb_device_desc_t descriptor={0x2dc8};*out=&descriptor;return ESP_OK;}
static esp_err_t usb_host_device_close(void *c,void *d){(void)c;(void)d;return ESP_OK;}
static esp_err_t usb_host_endpoint_halt(void *d,unsigned e){(void)d;(void)e;return ESP_OK;}
static esp_err_t usb_host_endpoint_flush(void *d,unsigned e){(void)d;(void)e;return ESP_OK;}
static esp_err_t usb_host_endpoint_clear(void *d,unsigned e){(void)d;(void)e;return ESP_OK;}
static const char *esp_err_to_name(esp_err_t e){(void)e;return "injected";}

