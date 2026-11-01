#include "libusb.h"
#include <usb/usb_host.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <new>
#include <algorithm>
struct libusb_device {libusb_context* context;uint8_t address;};
struct libusb_context {usb_host_client_handle_t client=nullptr;libusb_device devices[16]{};libusb_device* list[17]{};libusb_device_handle* opened=nullptr;};
struct libusb_device_handle {libusb_device* device;usb_device_handle_t handle=nullptr;usb_transfer_t* control=nullptr;usb_transfer_t* bulk=nullptr;bool gone=false,claimed=false,done=false,failed=false;};
static int(*cancelled)()=nullptr;
static void(*stalled)()=nullptr;
extern "C" void tab5_rtl_stall_callback(void(*cb)()){stalled=cb;}
extern "C" void tab5_rtl_cancel_callback(int(*cb)()){cancelled=cb;}
static void events(const usb_host_client_event_msg_t* e,void* arg){auto* c=static_cast<libusb_context*>(arg);if(e->event==USB_HOST_CLIENT_EVENT_DEV_GONE&&c->opened&&e->dev_gone.dev_hdl==c->opened->handle)c->opened->gone=true;}
extern "C" int libusb_init(libusb_context** result){auto* c=new(std::nothrow)libusb_context;if(!c)return -1;usb_host_client_config_t cfg{};cfg.is_synchronous=false;cfg.max_num_event_msg=16;cfg.async.client_event_callback=events;cfg.async.callback_arg=c;if(usb_host_client_register(&cfg,&c->client)!=ESP_OK){delete c;return -1;}*result=c;return 0;}
extern "C" void libusb_exit(libusb_context* c){if(c){usb_host_client_deregister(c->client);delete c;}}
extern "C" ssize_t libusb_get_device_list(libusb_context* c,libusb_device*** out){uint8_t addresses[16];int count=0;if(usb_host_device_addr_list_fill(16,addresses,&count)!=ESP_OK)return -1;for(int i=0;i<count;++i){c->devices[i]={c,addresses[i]};c->list[i]=&c->devices[i];}c->list[count]=nullptr;*out=c->list;return count;}
extern "C" void libusb_free_device_list(libusb_device**,int){}
extern "C" int libusb_get_device_descriptor(libusb_device* d,struct libusb_device_descriptor* out){memset(out,0,sizeof(*out));usb_device_handle_t h;if(usb_host_device_open(d->context->client,d->address,&h)!=ESP_OK)return -1;const usb_device_desc_t* desc=nullptr;int r=usb_host_get_device_descriptor(h,&desc);if(r==ESP_OK&&desc)memcpy(out,desc,sizeof(*out));usb_host_device_close(d->context->client,h);return r==ESP_OK?0:-1;}
extern "C" int libusb_open(libusb_device* d,libusb_device_handle** out){if(d->context->opened)return -1;auto* h=new(std::nothrow)libusb_device_handle;if(!h)return -1;h->device=d;if(usb_host_device_open(d->context->client,d->address,&h->handle)!=ESP_OK){delete h;return -1;}if(usb_host_transfer_alloc(512,0,&h->control)!=ESP_OK||usb_host_transfer_alloc(4096,0,&h->bulk)!=ESP_OK){if(h->control)usb_host_transfer_free(h->control);usb_host_device_close(d->context->client,h->handle);delete h;return -1;}d->context->opened=h;*out=h;return 0;}
extern "C" int libusb_claim_interface(libusb_device_handle* h,int i){if(i!=0)return -1;const usb_config_desc_t* config=nullptr;if(usb_host_get_active_config_descriptor(h->handle,&config)!=ESP_OK)return -1;const uint8_t* p=reinterpret_cast<const uint8_t*>(config);bool endpoint=false;unsigned iface=255,alternate=255;for(size_t offset=0;offset+2<=config->wTotalLength;){uint8_t n=p[offset],kind=p[offset+1];if(n<2||offset+n>config->wTotalLength)return -1;if(kind==4&&n>=9){iface=p[offset+2];alternate=p[offset+3];}if(kind==5&&n>=7&&iface==0&&alternate==0&&p[offset+2]==0x81&&(p[offset+3]&3)==2)endpoint=true;offset+=n;}if(!endpoint)return -1;int r=usb_host_interface_claim(h->device->context->client,h->handle,0,0);h->claimed=r==ESP_OK;return h->claimed?0:-1;}
extern "C" int libusb_release_interface(libusb_device_handle* h,int){if(h->claimed){h->claimed=false;return usb_host_interface_release(h->device->context->client,h->handle,0)==ESP_OK?0:-1;}return 0;}
extern "C" void libusb_close(libusb_device_handle* h){if(!h)return;libusb_release_interface(h,0);usb_host_transfer_free(h->control);usb_host_transfer_free(h->bulk);usb_host_device_close(h->device->context->client,h->handle);h->device->context->opened=nullptr;delete h;}
extern "C" libusb_device* libusb_get_device(libusb_device_handle* h){return h->device;}
extern "C" int libusb_kernel_driver_active(libusb_device_handle*,int){return 0;}
extern "C" int libusb_reset_device(libusb_device_handle*){return LIBUSB_ERROR_NOT_SUPPORTED;}
static void done(usb_transfer_t* t){static_cast<libusb_device_handle*>(t->context)->done=true;}
static int submit(libusb_device_handle* h,usb_transfer_t* t,unsigned timeout,bool control){
 if(h->gone||h->failed||(cancelled&&cancelled()))return -1;h->done=false;t->device_handle=h->handle;t->callback=done;t->context=h;t->timeout_ms=0;
 esp_err_t result=control?usb_host_transfer_submit_control(h->device->context->client,t):usb_host_transfer_submit(t);if(result!=ESP_OK)return -1;
 // IDF 5.5 cannot cancel EP0 through endpoint_halt/flush. Keep its
 // DMA buffer alive until completion/disconnection, without resetting the
 // shared host (which may also have a mounted disk).
 // handle_events already waits for USB; do not add a tick after its completion.
 // Keep the cooperative delay only while DMA is still pending.
 TickType_t start=xTaskGetTickCount();bool aborting=false;while(!h->done){usb_host_client_handle_events(h->device->context->client,pdMS_TO_TICKS(5));if(!aborting&&(h->gone||(cancelled&&cancelled())||xTaskGetTickCount()-start>pdMS_TO_TICKS(timeout?timeout:3000))){aborting=true;h->failed=true;if(control){if(stalled&&!h->gone)stalled();}else{usb_host_endpoint_halt(h->handle,t->bEndpointAddress);if(usb_host_endpoint_flush(h->handle,t->bEndpointAddress)!=ESP_OK&&stalled&&!h->gone)stalled();}}if(!h->done)vTaskDelay(1);}
 if(aborting){if(!h->gone&&!control)usb_host_endpoint_clear(h->handle,t->bEndpointAddress);return -1;}return t->status==USB_TRANSFER_STATUS_COMPLETED?t->actual_num_bytes:-1;
}
extern "C" int libusb_control_transfer(libusb_device_handle* h,uint8_t type,uint8_t request,uint16_t value,uint16_t index,unsigned char* data,uint16_t length,unsigned timeout){if(!h||length>504||(length&&!data))return -1;auto* t=h->control;auto* p=t->data_buffer;p[0]=type;p[1]=request;p[2]=value;p[3]=value>>8;p[4]=index;p[5]=index>>8;p[6]=length;p[7]=length>>8;if(!(type&0x80)&&length)memcpy(p+8,data,length);t->bEndpointAddress=0;t->num_bytes=8+length;int r=submit(h,t,timeout,true);if(r<8||r>8+length)return -1;r-=8;if(type&0x80)memcpy(data,p+8,std::min<int>(length,r));return r;}
extern "C" int libusb_bulk_transfer(libusb_device_handle* h,unsigned char endpoint,unsigned char* data,int length,int* received,unsigned timeout){if(!received)return -1;*received=0;if(!h||!data||endpoint!=0x81||length<=0||length>4096||length%512)return -1;auto* t=h->bulk;t->bEndpointAddress=endpoint;t->num_bytes=length;int n=submit(h,t,timeout,false);if(n<0||n>length)return -1;memcpy(data,t->data_buffer,n);*received=n;return 0;}
extern "C" int libusb_get_string_descriptor_ascii(libusb_device_handle* h,uint8_t index,unsigned char* out,int capacity){if(!index||capacity<1)return -1;uint8_t buf[254];int n=libusb_control_transfer(h,0x80,6,0x300|index,0x409,buf,sizeof(buf),1000);if(n<2)return -1;int used=0;for(int i=2;i+1<n&&used+1<capacity;i+=2)out[used++]=buf[i+1]?'?':buf[i];out[used]=0;return used;}
