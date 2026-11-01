#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5Task.h"
#include "Tab5Firmware.h"
#include "Tab5FilePolicy.h"
#include <SD.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_app_desc.h>
#include <esp_chip_info.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <atomic>
#include <algorithm>
#include <cstring>
namespace Tab5Firmware { namespace {
std::atomic<bool> working{false},cancelled{false},verified{false},complete{false};
std::atomic<unsigned> percent{0};std::atomic<const char*> state{"Prêt"};
String filename,details;std::string reviewedPath;
uint8_t reviewedHash[32]{};size_t reviewedSize=0;bool installing=false,installingReviewed=false;
void perform(){
 Tab5StorageUse storage{true};if(!storage){state="Reconnexion SD en cours — réessayez";verified=false;return;}
 esp_ota_handle_t handle=0;bool active=false;
 auto fail=[&](const char* text){if(active)esp_ota_abort(handle);state=text;verified=false;};
 File input=tab5FS.open(filename,FILE_READ);
 if(!input||input.isDirectory()){fail("Lecture du fichier impossible");return;}
 const size_t size=input.size();const auto* target=esp_ota_get_next_update_partition(nullptr);const auto* running=esp_ota_get_running_partition();
 if(!target||target==running||size>target->size||size<sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t)+sizeof(esp_app_desc_t)){fail("Binaire trop grand ou partition OTA absente");return;}
 esp_image_header_t header{};esp_image_segment_header_t segment{};esp_app_desc_t desc{};esp_chip_info_t chip{};esp_chip_info(&chip);
 if(input.read((uint8_t*)&header,sizeof(header))!=sizeof(header)||input.read((uint8_t*)&segment,sizeof(segment))!=sizeof(segment)||input.read((uint8_t*)&desc,sizeof(desc))!=sizeof(desc)){fail("Binaire tronqué");return;}
 if(header.magic!=ESP_IMAGE_HEADER_MAGIC||header.chip_id!=ESP_CHIP_ID_ESP32P4||desc.magic_word!=ESP_APP_DESC_MAGIC_WORD||segment.data_len<sizeof(desc)||!header.segment_count||header.segment_count>16){fail("Choisir un binaire application ESP32-P4, sans bootloader");return;}
 if(chip.revision<header.min_chip_rev_full||(header.max_chip_rev_full!=0&&header.max_chip_rev_full!=65535&&chip.revision>header.max_chip_rev_full)){fail("Firmware incompatible avec cette révision du Tab5");return;}
 if(installingReviewed&&size!=reviewedSize){fail("Le fichier a changé : vérifier à nouveau");return;}
 if(!input.seek(0)){fail("Lecture du fichier impossible");return;}
 if(installing){if(esp_ota_begin(target,size,&handle)!=ESP_OK){fail("Initialisation OTA impossible");return;}active=true;}
 mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);int rc=mbedtls_sha256_starts(&sha,0);
 uint8_t buffer[4096],hash[32];size_t done=0;const char* error=nullptr;
 while(rc==0&&done<size){
  if(cancelled){error="Installation annulée";break;}
  size_t want=std::min(sizeof(buffer),size-done);size_t n=input.read(buffer,want);
  if(n!=want){error="Binaire tronqué";break;}
  rc=mbedtls_sha256_update(&sha,buffer,n);
  if(rc==0&&installing&&esp_ota_write(handle,buffer,n)!=ESP_OK){error="Écriture OTA impossible";break;}
  done+=n;percent=unsigned(done*95/size);vTaskDelay(1);
 }
 if(!error&&rc==0)rc=mbedtls_sha256_finish(&sha,hash);mbedtls_sha256_free(&sha);
 if(error||rc){fail(error?error:"Empreinte du firmware impossible");return;}
 if(installing){
  if(installingReviewed&&memcmp(reviewedHash,hash,32)){fail("Le fichier a changé : vérifier à nouveau");return;}
  if(cancelled){fail("Installation annulée");return;}
  esp_err_t result=esp_ota_end(handle);active=false;
  if(result!=ESP_OK){fail("Intégrité du firmware invalide");return;}
  if(cancelled){fail("Installation annulée");return;}
  if(esp_ota_set_boot_partition(target)!=ESP_OK){fail("Activation du firmware impossible");return;}
  complete=true;verified=false;state="Firmware installé — redémarrer pour l’activer";
 }else{
  memcpy(reviewedHash,hash,32);reviewedSize=size;Tab5Files::canonical(filename.c_str(),reviewedPath);
  char fingerprint[65];for(unsigned i=0;i<32;++i)snprintf(fingerprint+i*2,3,"%02x",hash[i]);
  char project[33]{},version[33]{};memcpy(project,desc.project_name,32);memcpy(version,desc.version,32);
  details=String(project)+" · "+version+"\n"+String(size)+" B\nSHA-256\n"+fingerprint;verified=true;state="Binaire compatible — prêt à installer";
 }
 percent=100;
}
void task(void*){perform();working.store(false,std::memory_order_release);tab5TaskCheckpoint("firmware");vTaskDelete(nullptr);}
bool launch(bool write){
 bool expected=false;if(!working.compare_exchange_strong(expected,true))return false;
 installing=write;cancelled=false;percent=0;state=write?"Installation du firmware…":"Vérification du firmware…";
 if(tab5CreateTask(task,"tab5_firmware",12288,nullptr,1,nullptr,0)!=pdPASS){working=false;state="Mémoire insuffisante";return false;}return true;
}
}
void clearReview(){if(!busy()&&!complete){verified=false;reviewedPath.clear();details="";state="Prêt";}}
bool inspect(const char* path){if(busy()||complete)return false;std::string canonical;
 if(!path||!Tab5Files::canonical(path,canonical)||canonical.size()<5||canonical.substr(canonical.size()-4)!=".bin")return false;
 filename=path;verified=false;reviewedPath.clear();details="";installingReviewed=false;return launch(false);}
bool install(const char* path){
 if(busy()||complete)return false;
 // Existing callers without a selection may install only their reviewed file.
 if(!path){if(!ready())return false;path=filename.c_str();}
 std::string canonical;
 if(!Tab5Files::canonical(path,canonical)||canonical.size()<5||canonical.substr(canonical.size()-4)!=".bin")return false;
 installingReviewed=verified.load()&&canonical==reviewedPath;
 filename=path;
 if(!installingReviewed){verified=false;details="";}
 return launch(true);
}
void cancel(){cancelled=true;}
bool busy(){return working.load(std::memory_order_acquire);}bool ready(){return !busy()&&verified;}bool installed(){return !busy()&&complete;}
unsigned progress(){return percent;}const char* status(){return state.load();}String description(){return busy()?String(""):details;}
void restart(){if(installed())esp_restart();}
}
