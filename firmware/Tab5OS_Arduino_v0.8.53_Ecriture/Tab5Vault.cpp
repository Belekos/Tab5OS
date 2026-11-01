#include "Tab5VolumeFS.h"
#include "Tab5AtomicFile.h"
#include "Tab5Storage.h"
#include "Tab5Task.h"
#include "Tab5Vault.h"
#include "Tab5VaultCore.h"
#include "Tab5FilePolicy.h"
#include "Tab5KeyStore.h"
#include <FS.h>
#include <SD.h>
#include "Tab5CryptoRandom.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>
#include <algorithm>
#include <mbedtls/md.h>
namespace {
std::atomic<bool> working{false},cancelled{false};std::atomic<unsigned> percent{0};std::atomic<const char*> message{"Prêt"};
String sourcePath,destinationPath;char secret[257]{};bool decrypting=false;
enum class Mode {Password,Key,Backup,Restore,DeviceBackup,DeviceRestore};Mode mode=Mode::Password;Tab5KeyStore::Record record;
uint8_t deviceMaster[32]{},deviceId[16]{};
uint32_t jobRevision=0;bool jobNeedsSession=false;
StaticSemaphore_t jobMutexStorage;
SemaphoreHandle_t jobMutex(){static auto m=xSemaphoreCreateMutexStatic(&jobMutexStorage);return m;}
struct JobGuard{JobGuard(){xSemaphoreTake(jobMutex(),portMAX_DELAY);}~JobGuard(){xSemaphoreGive(jobMutex());}};
bool captureSession(){jobNeedsSession=true;if(Tab5KeyStore::sessionToken(jobRevision))return true;message="Coffre verrouillé";return false;}
void wipeJob(){tab5vault::wipe(secret,sizeof(secret));tab5vault::wipe(&record,sizeof(record));tab5vault::wipe(deviceMaster,sizeof(deviceMaster));tab5vault::wipe(deviceId,sizeof(deviceId));}
bool deviceMaterial(){uint8_t master[32]{};if(!Tab5KeyStore::deviceKey(master,deviceId)){message=Tab5KeyStore::status();return false;}static const uint8_t domain[]="Tab5 local key backup v1";bool ok=mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),master,32,domain,sizeof(domain)-1,deviceMaster)==0;tab5vault::wipe(master,sizeof(master));if(!ok)message="Chiffrement indisponible";return ok;}
struct KeyMemory:tab5vault::Stream {
 uint8_t bytes[106]{};size_t length=0,position=0;
 ~KeyMemory(){tab5vault::wipe(bytes,sizeof(bytes));}
 size_t read(void* out,size_t n)override{n=std::min(n,length-position);memcpy(out,bytes+position,n);position+=n;return n;}
 size_t write(const void* in,size_t n)override{if(n>sizeof(bytes)-position)return 0;memcpy(bytes+position,in,n);position+=n;length=std::max(length,position);return n;}
 bool seek(size_t n)override{if(n>length)return false;position=n;return true;}size_t size()override{return length;}
};
struct SdStream:tab5vault::Stream {
 File file;explicit SdStream(const String& p,bool write):file(tab5FS.open(p,write?FILE_WRITE:FILE_READ)){}
 size_t read(void* p,size_t n)override{return n?file.read(static_cast<uint8_t*>(p),n):0;}
 size_t write(const void* p,size_t n)override{return n?file.write(static_cast<const uint8_t*>(p),n):0;}
 bool seek(size_t n)override{return file.seek(n);}size_t size()override{return file.size();}
};
void runVault(){
 if(jobNeedsSession&&!Tab5KeyStore::sessionCurrent(jobRevision)){message="Coffre verrouillé";return;}
 Tab5StorageUse storage;if(!storage){message="Reconnexion SD en cours — réessayez";return;}
 String temp=destinationPath+".tmp";const char* error=nullptr;bool destinationPresent=false,tempPresent=false;
 auto tick=[](size_t done,size_t total,void*)->bool{percent=total?unsigned(done*100ULL/total):0;vTaskDelay(1);return !cancelled.load()&&(!jobNeedsSession||Tab5KeyStore::sessionCurrent(jobRevision));};
 const bool backup=mode==Mode::Backup||mode==Mode::DeviceBackup,restore=mode==Mode::Restore||mode==Mode::DeviceRestore;
 if(restore){
  SdStream in(sourcePath,false);KeyMemory clear;
  if(!in.file||in.file.isDirectory())error="Lecture du fichier impossible";
  else if(in.size()!=(mode==Mode::DeviceRestore?tab5vault::KeyHeaderSize:tab5vault::HeaderSize)+sizeof(clear.bytes)+16)error="Sauvegarde de clé invalide";
  else error=mode==Mode::DeviceRestore?tab5vault::decryptKey(in,clear,deviceMaster,deviceId,tick):tab5vault::decrypt(in,clear,secret,tick);
  if(!error){
   if(clear.length!=sizeof(clear.bytes)||memcmp(clear.bytes,"T5KEY001",8))error="Sauvegarde de clé invalide";
   else {memcpy(&record,clear.bytes+8,sizeof(record));if(Tab5KeyStore::import(record)<0)error=Tab5KeyStore::status();}
  }
 }else if(!tab5AtomicProbe(destinationPath.c_str(),destinationPresent)||!tab5AtomicProbe(temp.c_str(),tempPresent))error="Écriture du fichier impossible";
 else if(destinationPresent||tempPresent)error="La destination existe déjà";
 else {
  SdStream in(sourcePath.length()?sourcePath:String("/"),false);if(!backup&&(!in.file||in.file.isDirectory()))error="Lecture du fichier impossible";
  else {
   SdStream out(temp,true);if(!out.file)error="Écriture du fichier impossible";
   else {
    uint8_t entropy[24]{};if((backup||!decrypting)&&!Tab5CryptoRandom::fill(entropy,sizeof(entropy)))error="Aléa cryptographique indisponible";
    if(!error){
    if(backup){KeyMemory clear;memcpy(clear.bytes,"T5KEY001",8);memcpy(clear.bytes+8,&record,sizeof(record));clear.length=sizeof(clear.bytes);error=mode==Mode::DeviceBackup?tab5vault::encryptKey(clear,out,deviceMaster,deviceId,entropy,tick):tab5vault::encrypt(clear,out,secret,entropy,tick);}
    else if(mode==Mode::Key)error=decrypting?tab5vault::decryptKey(in,out,record.key,record.id,tick):tab5vault::encryptKey(in,out,record.key,record.id,entropy,tick);
    else if(decrypting)error=tab5vault::decrypt(in,out,secret,tick);
    else error=tab5vault::encrypt(in,out,secret,entropy,tick);
    }
    errno=0;out.file.flush();if(!error&&errno)error="Écriture du fichier impossible";
    errno=0;out.file.close();if(!error&&errno)error="Écriture du fichier impossible";
    if(!error&&!cancelled&&(!tab5AtomicProbe(destinationPath.c_str(),destinationPresent)||destinationPresent||!tab5FS.rename(temp,destinationPath)))error="Écriture du fichier impossible";
    if(cancelled&&!error)error="Opération annulée";
   }
  }
  if(error)tab5FS.remove(temp);
 }
 if(cancelled)error="Opération annulée";
 wipeJob();message=error?error:backup?"Sauvegarde de clé chiffrée créée":restore?"Clé restaurée dans le Tab5":decrypting?"Déchiffrement terminé":"Copie chiffrée créée - original conservé";
}
void task(void*){
 runVault();wipeJob();tab5TaskCheckpoint("vault");working=false;vTaskDelete(nullptr);
}
bool launch(){cancelled=false;percent=0;working=true;
 if(tab5CreateTask(task,"tab5_vault",16384,nullptr,0,nullptr,0)!=pdPASS){working=false;wipeJob();message="Mémoire insuffisante";return false;}return true;
}
bool paths(const String& source,const String& dest){std::string a,b;
 if(!Tab5Files::canonical(source.c_str(),a)||!Tab5Files::canonical(dest.c_str(),b)||a==b||a=="/"||b=="/"){message="Chemins invalides";return false;}
 File in=tab5FS.open(source);if(!in||in.isDirectory()){message="Lecture du fichier impossible";return false;}
 if(tab5FS.exists(dest)||tab5FS.exists(dest+".tmp")){message="La destination existe déjà";return false;}return true;
}
}
namespace Tab5Vault {
bool start(const String& source,const String& destination,const char* password,bool decrypt){
 JobGuard guard;
 if(working||!password||strlen(password)>256||source==destination)return false;
 unsigned characters=0;for(const unsigned char* c=reinterpret_cast<const unsigned char*>(password);*c;++c)if((*c&0xc0)!=0x80)++characters;if(characters<12)return false;
 if(!paths(source,destination))return false;
 sourcePath=source;destinationPath=destination;strcpy(secret,password);decrypting=decrypt;mode=Mode::Password;jobNeedsSession=false;message="Dérivation du mot de passe…";return launch();}
bool startKey(const String& source,const String& dest,unsigned slot,bool decrypt){JobGuard guard;if(working||!captureSession()||!paths(source,dest))return false;
 if(!Tab5KeyStore::read(slot,record)){message=Tab5KeyStore::status();return false;}
 sourcePath=source;destinationPath=dest;decrypting=decrypt;mode=Mode::Key;message=decrypt?"Déchiffrement en cours…":"Chiffrement en cours…";return launch();}
int keyForFile(const String& source){SdStream in(source,false);uint8_t id[16];if(!in.file||!tab5vault::keyId(in,id))return -2;return Tab5KeyStore::find(id);}
bool backupKey(unsigned slot,const String& dest,const char* password){JobGuard guard;if(working||!password||strlen(password)<12||strlen(password)>256||!captureSession())return false;
 std::string path;if(!Tab5Files::canonical(dest.c_str(),path)||path=="/"){message="Chemin invalide";return false;}
 if(tab5FS.exists(dest)||tab5FS.exists(dest+".tmp")){message="La destination existe déjà";return false;}
 if(!Tab5KeyStore::read(slot,record)){message=Tab5KeyStore::status();return false;}
 sourcePath="";destinationPath=dest;strcpy(secret,password);mode=Mode::Backup;message="Sauvegarde de la clé…";return launch();}
bool restoreKey(const String& source,const char* password){JobGuard guard;if(working||!password||strlen(password)>256||!captureSession())return false;
 std::string p;if(!Tab5Files::canonical(source.c_str(),p)||p=="/"){message="Chemin invalide";return false;}
 sourcePath=source;destinationPath="";strcpy(secret,password);mode=Mode::Restore;message="Restauration de la clé…";return launch();}

bool backupDevice(unsigned slot,const String& dest){JobGuard guard;if(working||!captureSession())return false;std::string path;if(!Tab5Files::canonical(dest.c_str(),path)||path=="/"){message="Chemin invalide";return false;}if(tab5FS.exists(dest)||tab5FS.exists(dest+".tmp")){message="La destination existe déjà";return false;}if(!Tab5KeyStore::read(slot,record)){message=Tab5KeyStore::status();wipeJob();return false;}if(!deviceMaterial()){wipeJob();return false;}sourcePath="";destinationPath=dest;decrypting=false;mode=Mode::DeviceBackup;message="Sauvegarde de la clé…";return launch();}
bool restoreDevice(const String& source){JobGuard guard;if(working||!captureSession())return false;std::string path;if(!Tab5Files::canonical(source.c_str(),path)||path=="/"){message="Chemin invalide";return false;}if(!deviceMaterial()){wipeJob();return false;}sourcePath=source;destinationPath="";mode=Mode::DeviceRestore;message="Restauration de la clé…";return launch();}
bool busy(){return working.load();}void cancel(){cancelled=true;}const char* status(){return message.load();}unsigned progress(){return percent.load();}
}
