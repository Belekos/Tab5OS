#include "Tab5Serial.h"
#include "Tab5Storage.h"
#include <atomic>
#include <cstring>
#ifdef ARDUINO
#include <Arduino.h>
#include "Tab5UsbStorage.h"
#include "Tab5VolumeFS.h"
#endif
namespace {
constexpr unsigned Exclusive=1U<<31;
constexpr unsigned MaxAttempts=3;
std::atomic<unsigned> users{0},failures{0},recoveries{0};
std::atomic<unsigned> untrackedUsb{0};
std::atomic<int> lastError{0};
std::atomic<bool> requested{false},workerRequest{false};
std::atomic<bool> deliberateEject{false};
std::atomic<unsigned> completions{0};
#ifdef ARDUINO
std::atomic<TaskHandle_t> exclusiveOwner{nullptr};
#else
static thread_local bool exclusiveOwner = false;
#endif
uint32_t lastAttempt=0;unsigned attempts=0;
}
Tab5StorageUse::Tab5StorageUse(bool usbTracked):usbTracked_(usbTracked){acquire();}
bool Tab5StorageUse::acquire(){
 if(held_)return true;
 unsigned state=users.load(std::memory_order_acquire);
 while(!(state&Exclusive) && state<Exclusive-1){
  if(users.compare_exchange_weak(state,state+1,std::memory_order_acq_rel)){
   if(usbTracked_){held_=true;break;}
   unsigned usb=untrackedUsb.load(std::memory_order_acquire);
   while(!(usb&Exclusive)&&usb<Exclusive-1){if(untrackedUsb.compare_exchange_weak(usb,usb+1,std::memory_order_acq_rel)){held_=true;break;}}
   if(!held_)users.fetch_sub(1,std::memory_order_release);
   break;
  }
 }
 return held_;
}
Tab5StorageUse::~Tab5StorageUse(){release();}
void Tab5StorageUse::release(){if(held_){held_=false;if(!usbTracked_)untrackedUsb.fetch_sub(1,std::memory_order_release);users.fetch_sub(1,std::memory_order_release);}}
Tab5StorageRecovery::Tab5StorageRecovery(){unsigned idle=0;held_=users.compare_exchange_strong(idle,Exclusive,std::memory_order_acq_rel);
 if(held_){
#ifdef ARDUINO
  exclusiveOwner.store(xTaskGetCurrentTaskHandle(),std::memory_order_release);
#else
  exclusiveOwner=true;
#endif
 }
}
Tab5StorageRecovery::~Tab5StorageRecovery(){if(held_){
#ifdef ARDUINO
 exclusiveOwner.store(nullptr,std::memory_order_release);
#else
 exclusiveOwner=false;
#endif
 users.store(0,std::memory_order_release);
}}
bool tab5StorageExclusiveOwner(){
#ifdef ARDUINO
 return exclusiveOwner.load(std::memory_order_acquire)==xTaskGetCurrentTaskHandle();
#else
 return exclusiveOwner;
#endif
}
void tab5StorageEjected(){deliberateEject=true;requested=false;workerRequest=false;attempts=0;lastError=ENODEV;}
Tab5UsbStorageRecovery::Tab5UsbStorageRecovery(){
 unsigned state=users.load(std::memory_order_acquire);
 while(!(state&Exclusive)&&state<Exclusive-1){
  if(users.compare_exchange_weak(state,state+1,std::memory_order_acq_rel)){
   unsigned idle=0;held_=untrackedUsb.compare_exchange_strong(idle,Exclusive,std::memory_order_acq_rel);
   if(!held_)users.fetch_sub(1,std::memory_order_release);
   break;
  }
 }
}
Tab5UsbStorageRecovery::~Tab5UsbStorageRecovery(){if(held_){untrackedUsb.store(0,std::memory_order_release);users.fetch_sub(1,std::memory_order_release);}}
void tab5StorageBootResult(bool mounted,uint32_t now){
 deliberateEject=false;
 attempts=0;lastAttempt=now;requested=!mounted;
 lastError=mounted?0:ENODEV;
 if(!mounted)++failures;
}
void tab5StorageRequestRecovery(){deliberateEject=false;attempts=0;lastAttempt=0;requested=true;}
bool tab5StorageTransportError(int error){return error==EIO||error==ENODEV||error==ENXIO||error==ETIMEDOUT;}
void tab5StorageFault(const char* operation,const char* path,int error){
 if(!error)error=EIO;
#ifdef ARDUINO
 const char* logical=path&&!strncmp(path,"/sd/",4)?path+3:path;
 if(Tab5UsbStorage::path(logical)){Tab5UsbStorage::fault(error);Tab5DebugSerial.printf("[USB] %s errno=%d\n",operation?operation:"I/O",error);return;}
#endif
 lastError=error;++failures;
 // Resource/path errors and invalid descriptors are not repaired by remounting.
 if(!deliberateEject&&tab5StorageTransportError(error))requested=true;
#ifdef ARDUINO
 Tab5DebugSerial.printf("[SD][ERREUR] %s | %s | errno=%d (%s)\n",operation?operation:"SD",path?path:"",error,tab5StorageErrorText());
#else
 (void)operation;(void)path;
#endif
}
void tab5StorageRequestRecoveryFromWorker(){deliberateEject=false;workerRequest.store(true,std::memory_order_release);}
unsigned tab5StorageRecoveryCompletions(){return completions.load(std::memory_order_acquire);}
bool tab5StorageRecoveryDue(uint32_t now){if(workerRequest.exchange(false)){attempts=0;lastAttempt=now-3000U;requested=true;}return requested.load()&&attempts<MaxAttempts&&uint32_t(now-lastAttempt)>=(attempts?30000U:3000U);}
void tab5StorageRecoveryDone(uint32_t now,bool success,bool retry){
 lastAttempt=now;
 if(success){++recoveries;requested=false;attempts=0;lastError=0;}
 else {if(attempts<MaxAttempts)++attempts;if(!retry)requested=false;}
 completions.fetch_add(1,std::memory_order_release);
}
unsigned tab5StorageRecoveryAttempts(){return attempts;}
bool tab5StorageRecoveryExhausted(){return requested.load()&&attempts>=MaxAttempts;}
int tab5StorageLastError(){return lastError.load();}
unsigned tab5StorageFailures(){return failures.load();}
unsigned tab5StorageRecoveries(){return recoveries.load();}
const char* tab5StorageErrorText(){switch(lastError.load()){
 case 0:return "SD prête";case ENOSPC:return "Carte SD pleine";case EMFILE:case ENFILE:return "Trop de fichiers ouverts";
 case ENOMEM:return "Mémoire insuffisante";case EEXIST:return "Destination déjà présente";
 case EBUSY:return "Carte SD occupée";case ENOENT:return "Chemin introuvable";
 case ENODEV:case ENXIO:return "Carte SD non détectée";case ETIMEDOUT:return "Carte SD sans réponse";
 case EACCES:case EROFS:return "Écriture non autorisée";default:return "Erreur de communication ou d’écriture SD";
}}
void tab5StorageLog(){
#ifdef ARDUINO
 Tab5DebugSerial.printf("[SD] echecs=%u, recuperations=%u, utilisateurs=%u, fichiers=%u, exclusif=%u, disponible=%u, errno=%d, attente=%d, essais=%u/%u, limite=%d\n",failures.load(),recoveries.load(),users.load()&~Exclusive,tab5SdOpenFiles(),unsigned(bool(users.load()&Exclusive)),unsigned(tab5SdAvailable()),lastError.load(),int(requested.load()),attempts,MaxAttempts,int(tab5StorageRecoveryExhausted()));
#endif
}
