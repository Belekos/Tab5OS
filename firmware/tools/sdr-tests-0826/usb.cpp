#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <new>
#include <unordered_set>
using ssize_t=ptrdiff_t;
#define __attribute__(x)
#include "src/rtl_sdr/libusb.h"
static unsigned checks=0;static void check(bool ok,const char* msg){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",msg);exit(1);}}
using esp_err_t=int;using TickType_t=uint32_t;using usb_device_handle_t=void*;using usb_host_client_handle_t=void*;
constexpr int ESP_OK=0,USB_HOST_CLIENT_EVENT_DEV_GONE=1,USB_TRANSFER_STATUS_COMPLETED=0;static uint32_t ticks=0;static unsigned sleepCalls=0;
unsigned pdMS_TO_TICKS(unsigned n){return n;}TickType_t xTaskGetTickCount(){return ticks;}void vTaskDelay(unsigned n){ticks+=n;++sleepCalls;}
struct usb_transfer_t {usb_device_handle_t device_handle=nullptr;uint8_t* data_buffer=nullptr;int bEndpointAddress=0,num_bytes=0,actual_num_bytes=0,status=0;unsigned timeout_ms=0;void(*callback)(usb_transfer_t*)=nullptr;void* context=nullptr;};
struct usb_host_client_event_msg_t {int event=0;struct {usb_device_handle_t dev_hdl=nullptr;}dev_gone;};
struct usb_host_client_config_t {bool is_synchronous=false;unsigned max_num_event_msg=0;struct{void(*client_event_callback)(const usb_host_client_event_msg_t*,void*)=nullptr;void* callback_arg=nullptr;}async;};
using usb_device_desc_t=libusb_device_descriptor;struct usb_config_desc_t{uint8_t length=9,type=2;uint16_t wTotalLength=25;uint8_t bytes[21]{};};
static usb_host_client_config_t clientConfig;static usb_config_desc_t config;static usb_device_desc_t descriptor;static unsigned openRefs=0,transferAllocations=0,failAllocation=0,eventCalls=0,completeAt=1,halted=0,flushed=0,stalls=0;static bool cancelNow=false,injectCancel=false,injectGone=false;static int actualOverride=-1;static usb_transfer_t* pending=nullptr;static std::unordered_set<usb_transfer_t*> transfers;
int usb_host_client_register(const usb_host_client_config_t* cfg,usb_host_client_handle_t* h){clientConfig=*cfg;*h=(void*)1;return 0;}
int usb_host_client_deregister(usb_host_client_handle_t){check(openRefs==0&&transfers.empty()&&!pending,"deregister only after USB resources drained");return 0;}
int usb_host_device_addr_list_fill(unsigned,uint8_t* addresses,int* count){*count=1;addresses[0]=1;return 0;}
int usb_host_device_open(usb_host_client_handle_t,uint8_t,usb_device_handle_t* h){++openRefs;*h=(void*)2;return 0;}
int usb_host_device_close(usb_host_client_handle_t,usb_device_handle_t){check(openRefs>0&&!pending,"close no outstanding DMA");--openRefs;return 0;}
int usb_host_get_device_descriptor(usb_device_handle_t,const usb_device_desc_t** desc){*desc=&descriptor;return 0;}
int usb_host_get_active_config_descriptor(usb_device_handle_t,const usb_config_desc_t** desc){*desc=&config;return 0;}
int usb_host_transfer_alloc(unsigned size,int,usb_transfer_t** out){++transferAllocations;if(failAllocation==transferAllocations)return -1;auto* t=new usb_transfer_t;t->data_buffer=new uint8_t[size]{};transfers.insert(t);*out=t;return 0;}
int usb_host_transfer_free(usb_transfer_t* t){check(t!=pending&&transfers.erase(t)==1,"free only completed owned DMA transfer");delete[] t->data_buffer;delete t;return 0;}
int usb_host_interface_claim(usb_host_client_handle_t,usb_device_handle_t,int,int){return 0;}int usb_host_interface_release(usb_host_client_handle_t,usb_device_handle_t,int){check(!pending,"release interface only after completion");return 0;}
int usb_host_transfer_submit(usb_transfer_t* t){check(!pending,"single in-flight transfer");pending=t;eventCalls=0;return 0;}
int usb_host_transfer_submit_control(usb_host_client_handle_t,usb_transfer_t* t){return usb_host_transfer_submit(t);}
int usb_host_client_handle_events(usb_host_client_handle_t,unsigned){++ticks;++eventCalls;if(injectCancel&&eventCalls==2)cancelNow=true;if(injectGone&&eventCalls==2){usb_host_client_event_msg_t e;e.event=USB_HOST_CLIENT_EVENT_DEV_GONE;e.dev_gone.dev_hdl=(void*)2;clientConfig.async.client_event_callback(&e,clientConfig.async.callback_arg);}if(pending&&eventCalls>=completeAt){auto* t=pending;pending=nullptr;t->actual_num_bytes=actualOverride>=0?actualOverride:t->num_bytes;t->status=0;t->callback(t);}return 0;}
int usb_host_endpoint_halt(usb_device_handle_t,int){++halted;return 0;}int usb_host_endpoint_flush(usb_device_handle_t,int){++flushed;return 0;}int usb_host_endpoint_clear(usb_device_handle_t,int){return 0;}
/* PRODUCTION */
static int stopRequested(){return cancelNow;}static void mockStalled(){++stalls;}
static void reset(){check(openRefs==0&&transfers.empty()&&!pending,"previous USB cycle clean");ticks=0;sleepCalls=0;transferAllocations=failAllocation=eventCalls=halted=flushed=stalls=0;completeAt=1;cancelNow=injectCancel=injectGone=false;actualOverride=-1;config=usb_config_desc_t{};auto* p=reinterpret_cast<uint8_t*>(&config);uint8_t bytes[]={9,2,25,0,1,1,0,0x80,50,9,4,0,0,1,0xff,0,0,0,7,5,0x81,2,0,2,0};memcpy(p,bytes,sizeof(bytes));tab5_rtl_cancel_callback(stopRequested);tab5_rtl_stall_callback(mockStalled);}
static libusb_device_handle* open(libusb_context*& context){check(libusb_init(&context)==0,"context initialized");libusb_device** list=nullptr;check(libusb_get_device_list(context,&list)==1,"device enumeration");libusb_device_handle* h=nullptr;int r=libusb_open(list[0],&h);if(failAllocation)check(r<0&&!h,"DMA allocation failure propagated");else check(r==0&&h,"device opens");return h;}
struct rtlsdr_dev_t {int direct_sampling=0;uint32_t freq=0;};static uint32_t retuned=0;static unsigned registerWrites=0;static int failRegister=0;
int rtlsdr_set_center_freq(rtlsdr_dev_t* dev,uint32_t f){dev->freq=f;retuned=f;return 0;}
int rtlsdr_set_direct_sampling(rtlsdr_dev_t* dev,int input){dev->direct_sampling=input;return rtlsdr_set_center_freq(dev,dev->freq);}
constexpr int USBB=0,USB_EPA_CTL=0;int rtlsdr_write_reg(rtlsdr_dev_t*,int,int,int,int){return ++registerWrites==failRegister?-1:0;}
/* INPUT_SETTER */
/* RESET_BUFFER */
int main(){
 for(unsigned failure=1;failure<=2;++failure){reset();failAllocation=failure;libusb_context* context;check(!open(context),"failed open has no handle");libusb_exit(context);check(transfers.empty()&&openRefs==0,"partial DMA allocation cleaned");}
 reset();libusb_context* context;auto* h=open(context);check(libusb_claim_interface(h,0)==0,"bulk endpoint on alternate zero accepted");uint8_t data[4096]{};int got=0;check(libusb_bulk_transfer(h,0x81,data,sizeof(data),&got,100)==0&&got==4096,"valid bounded bulk delivered");check(sleepCalls==0,"completed transfer has no unnecessary scheduler delay");actualOverride=4097;check(libusb_bulk_transfer(h,0x81,data,sizeof(data),&got,100)<0&&got==0,"oversized completion rejected before copy");actualOverride=30;check(libusb_control_transfer(h,0x80,0,0,0,data,8,100)<0,"control completion exceeds request rejected");libusb_close(h);libusb_exit(context);
 reset();reinterpret_cast<uint8_t*>(&config)[12]=1;h=open(context);check(libusb_claim_interface(h,0)<0,"endpoint only in alternate one not silently claimed as zero");libusb_close(h);libusb_exit(context);
 reset();h=open(context);check(libusb_claim_interface(h,0)==0,"claimed");completeAt=8;injectCancel=true;check(libusb_bulk_transfer(h,0x81,data,sizeof(data),&got,100)<0,"bulk cancel returns failure");check(eventCalls>=8&&halted==1&&flushed==1&&!pending,"bulk cancel drains before close");libusb_close(h);libusb_exit(context);
 reset();h=open(context);completeAt=8;injectCancel=true;check(libusb_control_transfer(h,0x80,0,0,0,data,8,100)<0,"EP0 cancel reports failure");check(eventCalls>=8&&halted==0&&flushed==0&&stalls==1&&!pending,"EP0 waits for callback without invalid halt/free");libusb_close(h);libusb_exit(context);
 reset();h=open(context);completeAt=8;injectGone=true;check(libusb_bulk_transfer(h,0x81,data,sizeof(data),&got,100)<0,"unplug propagates error");check(eventCalls>=8&&!pending,"unplug drains in-flight buffer");libusb_close(h);libusb_exit(context);
 for(unsigned i=0;i<2000;++i){reset();h=open(context);libusb_claim_interface(h,0);libusb_bulk_transfer(h,0x81,data,sizeof(data),&got,100);libusb_close(h);libusb_exit(context);}
 rtlsdr_dev_t dev;dev.freq=100000000;check(tab5_rtlsdr_set_input_and_freq(&dev,2,531000)==0&&retuned==531000&&dev.direct_sampling==2,"input switch retunes requested HF frequency not old VHF");registerWrites=0;failRegister=1;check(rtlsdr_reset_buffer(&dev)<0&&registerWrites==1,"first USB reset write error propagated");registerWrites=0;failRegister=2;check(rtlsdr_reset_buffer(&dev)<0&&registerWrites==2,"second USB reset write error propagated");
 printf("PASS %u SDR USB adapter checks; 2000 complete lifecycles; cancellation, unplug, EP0 completion, DMA OOM and bounds\n",checks);
}

