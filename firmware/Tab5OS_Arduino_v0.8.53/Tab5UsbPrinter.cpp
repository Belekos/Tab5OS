#include "Tab5UsbPrinter.h"
#include "Tab5UsbIppProtocol.h"
#include "Tab5Task.h"
#include "Tab5Memory.h"
#include <usb/usb_host.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <algorithm>
#include <cstring>

namespace Tab5UsbPrinter {namespace {
constexpr size_t TransferBytes=2048,ResponseBytes=65536;
usb_host_client_handle_t client=nullptr;
usb_device_handle_t device=nullptr;
usb_transfer_t* transfer=nullptr;
SemaphoreHandle_t gate=nullptr,completed=nullptr;
QueueHandle_t arrivals=nullptr;
Tab5UsbIppProtocol::Interface iface;
std::atomic<bool> usable{false},gone{false},poisoned{false};
std::atomic<uint32_t> generation{1},changed{0};
std::atomic<const char*> message{"Aucune imprimante USB compatible"};
void note(const char* text){if(message.exchange(text)!=text)++changed;}
void done(usb_transfer_t*){xSemaphoreGive(completed);}
void event(const usb_host_client_event_msg_t* event,void*){
 if(event->event==USB_HOST_CLIENT_EVENT_NEW_DEV){const uint8_t address=event->new_dev.address;xQueueSend(arrivals,&address,0);}
 else if(event->event==USB_HOST_CLIENT_EVENT_DEV_GONE&&event->dev_gone.dev_hdl==device){usable=false;gone=true;++generation;note("Imprimante USB débranchée");}
}
bool closeDevice(){
 usable=false;
 if(!device)return true;
 if(iface.valid&&usb_host_interface_release(client,device,iface.number)!=ESP_OK)return false;
 iface.valid=false;
 if(transfer){if(usb_host_transfer_free(transfer)!=ESP_OK)return false;transfer=nullptr;}
 if(usb_host_device_close(client,device)!=ESP_OK)return false;
 device=nullptr;iface={};gone=false;poisoned=false;return true;
}
void attach(uint8_t address){
 if(device)return;usb_device_handle_t candidate=nullptr;
 if(usb_host_device_open(client,address,&candidate)!=ESP_OK)return;
 const usb_config_desc_t* config=nullptr;
 if(usb_host_get_active_config_descriptor(candidate,&config)!=ESP_OK){usb_host_device_close(client,candidate);return;}
 const auto selected=Tab5UsbIppProtocol::interface(reinterpret_cast<const uint8_t*>(config),config->wTotalLength);
 if(!selected.valid){usb_host_device_close(client,candidate);return;}
 usb_transfer_t* buffer=nullptr;
 if(usb_host_transfer_alloc(TransferBytes,0,&buffer)!=ESP_OK){usb_host_device_close(client,candidate);note("Imprimante USB : mémoire insuffisante");return;}
 if(usb_host_interface_claim(client,candidate,selected.number,selected.alternate)!=ESP_OK){usb_host_transfer_free(buffer);usb_host_device_close(client,candidate);note("Interface d’impression USB indisponible");return;}
 device=candidate;transfer=buffer;iface=selected;gone=false;poisoned=false;++generation;usable=true;note("Imprimante USB IPP prête");
}
void task(void*){
 for(;;){
  usb_host_client_handle_events(client,pdMS_TO_TICKS(100));
  if(xSemaphoreTake(gate,0)!=pdTRUE)continue;
  if(gone)closeDevice();
  uint8_t address;while(xQueueReceive(arrivals,&address,0)==pdTRUE)attach(address);
  xSemaphoreGive(gate);
 }
}
bool alive(uint32_t token){return usable.load()&&!gone.load()&&!poisoned.load()&&token==generation.load();}
// The task servicing USB callbacks is independent of the printing worker.
// Waits are finite except for completion after endpoint flush: the transfer
// buffer must never be freed while still owned by the host controller.
int transaction(bool input,size_t bytes,uint32_t timeoutMs,uint32_t token){
 if(!alive(token)||!transfer||bytes>TransferBytes)return -1;
 while(xSemaphoreTake(completed,0)==pdTRUE){}
 transfer->device_handle=device;transfer->bEndpointAddress=input?iface.in:iface.out;
 transfer->num_bytes=input?TransferBytes:bytes;transfer->callback=done;transfer->context=nullptr;transfer->timeout_ms=timeoutMs;
 if(usb_host_transfer_submit(transfer)!=ESP_OK)return -1;
 if(xSemaphoreTake(completed,pdMS_TO_TICKS(timeoutMs))!=pdTRUE){
  usb_host_endpoint_halt(device,transfer->bEndpointAddress);
  usb_host_endpoint_flush(device,transfer->bEndpointAddress);
  usb_host_endpoint_clear(device,transfer->bEndpointAddress);
  xSemaphoreTake(completed,portMAX_DELAY);
  if(input&&alive(token)&&transfer->actual_num_bytes>=0&&size_t(transfer->actual_num_bytes)<=TransferBytes)return transfer->actual_num_bytes;
  return -1;
 }
 if(!alive(token)||transfer->status!=USB_TRANSFER_STATUS_COMPLETED||transfer->actual_num_bytes<0||size_t(transfer->actual_num_bytes)>TransferBytes)return -1;
 return transfer->actual_num_bytes;
}
struct Gate {
 bool held=false;
 explicit Gate(uint32_t wait){held=gate&&xSemaphoreTake(gate,pdMS_TO_TICKS(wait))==pdTRUE;}
 ~Gate(){if(held){xSemaphoreGive(gate);usb_host_client_unblock(client);}}
};
}
Response::~Response(){heap_caps_free(bytes_);}
bool Response::prepare(){if(!bytes_)bytes_=static_cast<uint8_t*>(heap_caps_malloc(ResponseBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));length_=0;return bytes_!=nullptr;}
bool begin(){
 if(client)return true;
 gate=xSemaphoreCreateMutex();completed=xSemaphoreCreateBinary();arrivals=xQueueCreate(8,sizeof(uint8_t));
 auto cleanup=[](){if(gate)vSemaphoreDelete(gate);if(completed)vSemaphoreDelete(completed);if(arrivals)vQueueDelete(arrivals);gate=completed=nullptr;arrivals=nullptr;};
 if(!gate||!completed||!arrivals){cleanup();note("Imprimante USB : mémoire insuffisante");return false;}
 usb_host_client_config_t config{};config.is_synchronous=false;config.max_num_event_msg=8;config.async.client_event_callback=event;config.async.callback_arg=nullptr;
 if(usb_host_client_register(&config,&client)!=ESP_OK){client=nullptr;cleanup();note("Impression USB indisponible");return false;}
 if(tab5CreateTask(task,"tab5_usb_printer",4096,nullptr,3,nullptr,0)!=pdPASS){usb_host_client_deregister(client);client=nullptr;cleanup();note("Imprimante USB : mémoire insuffisante");return false;}
 return true;
}
bool ready(){return usable.load()&&!gone.load()&&!poisoned.load();}
const char* status(){return message.load();}
uint32_t revision(){return changed.load();}
uint32_t connectionToken(){return generation.load();}
bool uri(const char* value){return value&&!strcmp(value,"usb://printer");}
bool post(Stream& payload,size_t bytes,Response& response,int& httpStatus,uint32_t expectedToken,uint32_t timeoutMs,bool(*cancel)(void*),void* context){
 httpStatus=0;
 if(!bytes||bytes>128U*1024*1024||timeoutMs<1000||timeoutMs>120000||!cancel||!alive(expectedToken)){note("Imprimante USB indisponible ou changée");return false;}
 // Admit PSRAM response + bounded internal scratch before the first USB byte.
 Tab5Resources::AdmissionReport report;
 auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,{4096,2048,0,ResponseBytes,ResponseBytes},"USB IPP",&report);
 if(!admission||!response.prepare()){note("Imprimante USB : mémoire insuffisante");return false;}
 admission.release();
 Gate lock(1000);if(!lock.held||!alive(expectedToken)){note("Imprimante USB indisponible ou changée");return false;}
 if(cancel(context))return false;
 bool sent=false;
 auto fail=[&](){if(sent&&usable.load()){poisoned=true;note("Imprimante USB : rebrancher après un envoi interrompu");}return false;};
 auto send=[&](const uint8_t* data,size_t count)->bool{
  while(count){if(cancel(context)||!alive(expectedToken))return false;const size_t n=std::min(count,TransferBytes);memcpy(transfer->data_buffer,data,n);sent=true;
   if(transaction(false,n,5000,expectedToken)!=int(n))return false;data+=n;count-=n;
  }return true;
 };
 char header[192];const int length=snprintf(header,sizeof(header),"POST /ipp/print HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/ipp\r\nContent-Length: %u\r\nConnection: keep-alive\r\n\r\n",unsigned(bytes));
 if(length<=0||size_t(length)>=sizeof(header)||!send(reinterpret_cast<const uint8_t*>(header),size_t(length)))return fail();
 // No second buffer: source bytes go directly into the driver's bounded DMA
 // transfer buffer. Payload's existing progress callback remains in use.
 for(size_t at=0;at<bytes;){
  if(cancel(context)||!alive(expectedToken))return fail();size_t n=std::min(bytes-at,TransferBytes);
  if(payload.readBytes(reinterpret_cast<char*>(transfer->data_buffer),n)!=n)return fail();
  if(transaction(false,n,5000,expectedToken)!=int(n))return fail();at+=n;vTaskDelay(1);
 }
 size_t offset=0,available=0;const uint32_t started=millis();
 auto next=[&]()->int{
  for(;;){
   if(cancel(context)||!alive(expectedToken)||uint32_t(millis()-started)>=timeoutMs)return -1;
   if(offset<available)return transfer->data_buffer[offset++];
   int n=transaction(true,TransferBytes,1000,expectedToken);if(n<0)return -1;
   offset=0;available=size_t(n);if(!n)vTaskDelay(1);
  }
 };
 const auto result=Tab5UsbIppProtocol::response(next,response,httpStatus);
 if(result!=Tab5UsbIppProtocol::Error::None||offset!=available)return fail();
 note("Imprimante USB IPP prête");return true;
}
}
