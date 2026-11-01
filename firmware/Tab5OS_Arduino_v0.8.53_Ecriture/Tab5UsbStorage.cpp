#include "Tab5UsbStorage.h"
#include "Tab5Storage.h"
#include "src/usb_msc/msc_host.h"
#include "src/usb_msc/msc_host_vfs.h"
#include "src/usb_msc/msc_scsi_bot.h"
#include "src/usb_msc/tab5_msc_format.h"
#include "Tab5Task.h"
#include <SD.h>
#include <atomic>
#include <cstring>
#include <cerrno>
#include <freertos/queue.h>
namespace Tab5UsbStorage { namespace {
std::atomic<QueueHandle_t> events{nullptr};
msc_host_device_handle_t device=nullptr;
msc_host_vfs_handle_t vfs=nullptr;
std::atomic<bool> present{false},ready{false},accept{false},autoMount{true},namespaceUsed{false};
std::atomic<unsigned> refs{0},request{0};
std::atomic<uint32_t> gen{1},rev{0};
std::atomic<uint32_t> connection{1},formatConnection{0};
std::atomic<msc_host_device_handle_t> trackedDevice{nullptr};
std::atomic<FormatState> formatting{FormatState::Idle};
std::atomic<bool> formatWorkerActive{false};
std::atomic<Format> requestedFormat{Format::Fat32};
std::atomic<uint32_t> capacitySectors{0},capacitySectorSize{0};
std::atomic<const char*> message{"Aucun stockage USB"};
uint8_t address=0;
void note(const char* text){if(message.exchange(text)!=text)++rev;}
void event(const msc_host_event_t* e,void*){
 if(e->event==msc_host_event_t::MSC_DEVICE_DISCONNECTED&&e->device.handle==trackedDevice.load()){present=false;accept=false;++connection;request=2;}
 if(xQueueSend(events,e,0)!=pdTRUE){accept=false;note("USB : trop de changements, rebrancher le support");}
}
bool sameFormatDevice(void*){return present.load()&&connection.load()==formatConnection.load();}
bool installDevice(){
 if(!device){
  if(msc_host_install_device(address,&device)!=ESP_OK){request=0;address=0;present=false;++connection;note("Montage USB impossible — rebrancher le support");return false;}
  trackedDevice=device;
 }
 msc_host_device_info_t info{};
 if(msc_host_get_device_info(device,&info)==ESP_OK){capacitySectors=info.sector_count;capacitySectorSize=info.sector_size;}
 else {capacitySectors=0;capacitySectorSize=0;}
 return true;
}
bool mountVfs(){
 esp_vfs_fat_mount_config_t cfg{};cfg.format_if_mount_failed=false;cfg.max_files=12;cfg.allocation_unit_size=0;
 if(msc_host_vfs_register(device,"/sd/USB",&cfg,&vfs)!=ESP_OK){note("Volume USB illisible : FAT16/FAT32 requis");return false;}
 namespaceUsed=true;++gen;ready=true;accept=present.load();note("Stockage USB prêt");return true;
}
void completeFormat(){
 unsigned expected=4;request.compare_exchange_strong(expected,0);
 if(!present)request=2;
}
void performFormat(){
 if(!sameFormatDevice(nullptr)){formatting=FormatState::Cancelled;note("Support USB changé : formatage annulé");return;}
 FormatState expected=FormatState::Waiting;
 if(!formatting.compare_exchange_strong(expected,FormatState::Running))return;
 note("Formatage USB en cours — ne pas débrancher");
 if(vfs){
  const esp_err_t sync=tab5_scsi_sync_cache(device);
  if((sync!=ESP_OK&&sync!=ESP_ERR_NOT_SUPPORTED)||msc_host_vfs_unregister(vfs)!=ESP_OK){formatting=FormatState::Failed;accept=present.load();note("Formatage USB impossible : erreur de fermeture");return;}
  vfs=nullptr;
 }
 ready=false;++gen;
 const esp_err_t result=tab5_msc_format_raw(device,requestedFormat.load()==Format::Fat32,sameFormatDevice,nullptr);
 if(result!=ESP_OK||!sameFormatDevice(nullptr)){
  formatting=FormatState::Failed;note(result==ESP_ERR_NO_MEM?"USB : mémoire insuffisante":"Formatage USB échoué — support non monté");return;
 }
 if(!SD.exists("/USB")&&mountVfs()){
  formatting=FormatState::Succeeded;note("Formatage USB terminé");
 }else{
  formatting=FormatState::Failed;note("USB formaté, mais montage impossible");
 }
}
void formatTask(void*){
 // Do not occupy the USB attach/recovery worker while FatFs creates metadata.
 // The device stays owned until this worker exits, including on disconnect.
 while(formatting.load()==FormatState::Waiting&&sameFormatDevice(nullptr)){
  {Tab5UsbStorageRecovery exclusive;
   if(exclusive&&!refs.load()){performFormat();break;}
  }
  note("Fermer les fichiers et arrêter les tâches utilisant le stockage");
  vTaskDelay(pdMS_TO_TICKS(100));
 }
 if(formatting.load()==FormatState::Waiting){formatting=FormatState::Cancelled;note("Support USB changé : formatage annulé");}
 completeFormat();
 tab5TaskCheckpoint("usb-format");
 // Status notes may have been observed while the exclusive USB lease was held
 // and pending() was true. Publish a final revision only once clients can open
 // the newly mounted volume; otherwise a failed UI refresh has no later event.
 formatWorkerActive.store(false,std::memory_order_release);++rev;vTaskDelete(nullptr);
}
}
bool path(const char* p){return namespaceUsed.load()&&p&&!strncmp(p,"/USB",4)&&(!p[4]||p[4]=='/');}
bool mounted(){return ready.load()&&present.load();}
bool attached(){return present.load();}
bool pending(){return request.load()!=0||formatWorkerActive.load();}
bool ejecting(){return request.load()==2;}
const char* status(){return message.load();}
uint32_t revision(){return rev.load();}
uint64_t capacityBytes(){return present?uint64_t(capacitySectors.load())*capacitySectorSize.load():0;}
uint32_t connectionToken(){return connection.load();}
bool supported(Format format){return present&&Tab5UsbFormatPolicy::geometry(format,capacitySectors.load(),capacitySectorSize.load());}
FormatState formatState(){return formatting.load();}
bool requestFormat(Format format,uint32_t expectedToken){
 if(!present||!expectedToken||expectedToken!=connection.load()||!supported(format)||pending())return false;
 requestedFormat=format;formatConnection=expectedToken;formatting=FormatState::Waiting;accept=false;
 unsigned idle=0;
 if(!request.compare_exchange_strong(idle,4)){formatting=FormatState::Idle;accept=mounted();return false;}
 if(!sameFormatDevice(nullptr)){formatting=FormatState::Cancelled;request=2;return false;}
 note("Formatage USB en attente");return true;
}
bool cancelFormat(){
 FormatState expected=FormatState::Waiting;
 if(!formatting.compare_exchange_strong(expected,FormatState::Cancelled))return false;
 unsigned action=4;request.compare_exchange_strong(action,0);accept=mounted();note("Formatage USB annulé");return true;
}
void mount(){if(present&&!pending()){accept=false;request=1;note("Montage USB en cours");}}
void eject(){if(ready&&!pending()){accept=false;request=2;note("Éjection USB en attente de fermeture des fichiers");}}
void fault(int){note("Erreur de lecture ou d’écriture USB");}
void automatic(bool enabled){autoMount=enabled;if(enabled&&present&&!ready)mount();}
bool automatic(){return autoMount;}
bool retain(uint32_t& generation){if(!accept||!mounted()){errno=ENODEV;return false;}++refs;if(!accept||!mounted()){--refs;errno=ENODEV;return false;}generation=gen;return true;}
void release(){--refs;}
bool valid(uint32_t generation){if(generation==gen.load()&&mounted())return true;errno=ENODEV;return false;}
bool begin(){
 if(events)return true;
 events=xQueueCreate(8,sizeof(msc_host_event_t));if(!events){note("USB : mémoire insuffisante");return false;}
 msc_host_driver_config_t cfg{};cfg.create_backround_task=true;cfg.task_priority=3;cfg.stack_size=4096;cfg.core_id=0;cfg.callback=event;
 if(msc_host_install(&cfg)!=ESP_OK){vQueueDelete(events);events=nullptr;note("Stockage USB indisponible");return false;}
 return true;
}
void serviceWorker(){
 if(!events)return;
 msc_host_event_t e;
 while(xQueueReceive(events,&e,0)==pdTRUE){
  if(e.event==msc_host_event_t::MSC_DEVICE_CONNECTED){
   if(device||address){note("Un seul stockage USB peut être monté à la fois");continue;}
   address=e.device.address;present=true;++connection;formatting=FormatState::Idle;capacitySectors=0;capacitySectorSize=0;note("Stockage USB détecté");request=autoMount?1:3;
  }else if(device==e.device.handle){present=false;accept=false;request=2;note("Stockage USB débranché");}
 }
 if(formatWorkerActive.load(std::memory_order_acquire))return;
 unsigned action=request.load();if(!action)return;
 if(action==4){
  if(formatting.load()!=FormatState::Waiting){completeFormat();return;}
  formatWorkerActive=true;
  if(tab5CreateTask(formatTask,"tab5_usb_format",8192,nullptr,1,nullptr,0)!=pdPASS){formatWorkerActive=false;formatting=FormatState::Failed;completeFormat();accept=mounted();note("USB : mémoire insuffisante");}
  return;
 }
 if(action==2){
  accept=false;
  Tab5UsbStorageRecovery exclusive;
  if(!exclusive||refs.load()){if(present)note("Fermer les fichiers et arrêter les tâches utilisant le stockage");return;}
  if(vfs&&present){
   esp_err_t sync=tab5_scsi_sync_cache(device);
   if(sync!=ESP_OK&&sync!=ESP_ERR_NOT_SUPPORTED){request=0;accept=true;note("Éjection impossible : erreur d’écriture USB");return;}
  }
  if(vfs){
   if(msc_host_vfs_unregister(vfs)!=ESP_OK){request=0;accept=present.load();note("Éjection impossible : erreur de fermeture USB");return;}
   vfs=nullptr;
  }ready=false;++gen;
  if(!present&&device){msc_host_uninstall_device(device);device=nullptr;trackedDevice=nullptr;address=0;capacitySectors=0;capacitySectorSize=0;}
  request=0;note(present?"Support USB éjecté — retrait possible":"Stockage USB débranché");return;
 }
 if(!present){request=0;return;}
 if(ready){request=0;accept=true;note("Stockage USB prêt");return;}
 if(!installDevice())return;
 if(action==3){request=0;return;}
 // Never shadow an existing personal directory. Automatic mount never formats.
 if(SD.exists("/USB")){request=0;note("Le nom USB est déjà utilisé sur la microSD");return;}
 mountVfs();
 unsigned expected=action;request.compare_exchange_strong(expected,0);++rev;
}
}
