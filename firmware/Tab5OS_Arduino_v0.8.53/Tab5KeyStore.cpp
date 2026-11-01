#include "Tab5KeyStore.h"
#include "Tab5VaultCore.h"
#include "Tab5Vault.h"
#include "Tab5Secrets.h"
#include "Tab5Task.h"
#include "Tab5Diagnostics.h"
#include <mbedtls/md.h>
#include <Preferences.h>
#include "Tab5CryptoRandom.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <new>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif
namespace {
using Tab5KeyStore::Record;
constexpr unsigned Count=Tab5KeyStore::MaxKeys;
constexpr size_t RingBytes=8+Count*(sizeof(Record)+1);
constexpr size_t CipherBytes=tab5vault::KeyHeaderSize+RingBytes+16;
struct Ring{uint8_t magic[8]={'T','5','K','E','Y','S','0','2'};uint8_t used[Count]{};Record keys[Count];};
static_assert(sizeof(Ring)==RingBytes,"Stable key store format");
struct Protected{uint8_t salt[16]{},id[16]{},cipher[CipherBytes]{};};
struct Secrets{uint8_t master[32]{};Ring ring;~Secrets(){tab5vault::wipe(this,sizeof(*this));}};
struct DeviceEnvelope{uint8_t magic[8]={'T','5','D','E','V','0','0','1'};uint8_t master[32]{};Protected data;~DeviceEnvelope(){tab5vault::wipe(this,sizeof(*this));}};
// Open/save also run from LVGL callbacks. Keep envelope verification copies
// off the UI stack, including error paths and repeated backup/open cycles.
template<class T>struct Scratch{T* value=nullptr;Scratch(){
#ifdef ARDUINO
 void* p=heap_caps_malloc(sizeof(T),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
 void* p=malloc(sizeof(T));
#endif
 if(p)value=new(p)T;
 }~Scratch(){if(value){value->~T();tab5vault::wipe(value,sizeof(T));
#ifdef ARDUINO
 heap_caps_free(value);
#else
 free(value);
#endif
 }}T& operator*(){return *value;}explicit operator bool()const{return value!=nullptr;}};
Secrets session;Protected envelope;bool deviceMode=false,sessionPinProtected=false;
std::atomic<bool> isOpen{false};uint32_t lastUse=0;
struct PinRecord{uint8_t magic[8]={'T','5','P','I','N','0','0','1'},salt[16]{},digest[32]{};};
uint32_t pinRetryAt=0;unsigned pinFailures=0;
enum class DeviceJob { Open, Initialize, ChangePin, Reset };
DeviceJob deviceJob=DeviceJob::Open;
char requestedPin[13]{},currentPin[13]{};bool requestedMigration=false;
std::atomic<bool> operationOk{false};
std::atomic<bool> uiContinuation{false};
struct ResetRecord { uint8_t magic[8]={'T','5','R','S','T','0','0','1'};char archive[64]{}; };
bool validReset(const ResetRecord& r){
 if(memcmp(r.magic,"T5RST001",8)||!memchr(r.archive,0,sizeof(r.archive))||strlen(r.archive)!=40||strncmp(r.archive,"/System/Secrets-retired-",24))return false;
 for(unsigned i=24;i<40;++i)if(!((r.archive[i]>='0'&&r.archive[i]<='9')||(r.archive[i]>='a'&&r.archive[i]<='f')))return false;return true;
}
std::atomic<bool> working{false};std::atomic<uint32_t> generation{0};
std::atomic<const char*> message{"Coffre verrouillé"};
char password[257]{};bool changing=false;uint32_t jobGeneration=0;
StaticSemaphore_t mutexStorage;
SemaphoreHandle_t mutex(){static auto m=xSemaphoreCreateMutexStatic(&mutexStorage);return m;}
struct Guard{Guard(){xSemaphoreTake(mutex(),portMAX_DELAY);}~Guard(){xSemaphoreGive(mutex());}};
// UI status polling must never wait for NVS/crypto in a worker. Actual key
// access keeps the blocking guard and rechecks authorization under that lock.
struct StatusGuard{bool held=xSemaphoreTake(mutex(),0);~StatusGuard(){if(held)xSemaphoreGive(mutex());}};
struct Memory:tab5vault::Stream{
 uint8_t* bytes;size_t capacity,length,position=0;
 Memory(void* p,size_t n,bool input):bytes(static_cast<uint8_t*>(p)),capacity(n),length(input?n:0){}
 size_t read(void* p,size_t n)override{n=std::min(n,length-position);memcpy(p,bytes+position,n);position+=n;return n;}
 size_t write(const void* p,size_t n)override{if(n>capacity-position)return 0;memcpy(bytes+position,p,n);position+=n;length=std::max(length,position);return n;}
 bool seek(size_t n)override{if(n>length)return false;position=n;return true;}size_t size()override{return length;}
};
void slotName(unsigned slot,char* out){snprintf(out,8,"key%u",slot);}
bool valid(const Record& r){return r.version==1&&r.name[0]&&memchr(r.name,0,sizeof(r.name));}
bool validRing(const Ring& r){if(memcmp(r.magic,"T5KEYS02",8))return false;for(unsigned i=0;i<Count;++i){if(r.used[i]>1||(r.used[i]&&!valid(r.keys[i])))return false;for(unsigned j=0;j<i;++j)if(r.used[i]&&r.used[j]&&!memcmp(r.keys[i].id,r.keys[j].id,16))return false;}return true;}
void clearSession(){tab5vault::wipe(&session,sizeof(session));isOpen=false;sessionPinProtected=false;lastUse=0;}
bool authorized(){return isOpen&&(!deviceMode||sessionPinProtected);}
bool stored(const char* name="vault2"){Preferences p;if(!p.begin("tab5keys",true))return false;bool exists=p.isKey(name);p.end();return exists;}
bool validPin(const char* pin){if(!pin)return false;size_t n=strlen(pin);if(n<4||n>12)return false;for(size_t i=0;i<n;++i)if(pin[i]<'0'||pin[i]>'9')return false;return true;}
bool pinHash(const char* pin,const uint8_t master[32],const uint8_t salt[16],uint8_t out[32]){
 uint8_t input[36]{};memcpy(input,"T5PIN001",8);memcpy(input+8,salt,16);memcpy(input+24,pin,strlen(pin));
 bool ok=mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),master,32,input,sizeof(input),out)==0;tab5vault::wipe(input,sizeof(input));return ok;
}
bool verifyPin(Preferences& p,const char* pin,const uint8_t master[32]){
 if(pinRetryAt&&int32_t(pinRetryAt-millis())>0){message="Trop de tentatives : patienter";return false;}
 PinRecord savedPin;uint8_t digest[32]{};
 bool valid=validPin(pin)&&p.getBytesLength("pin4")==sizeof(savedPin)&&p.getBytes("pin4",&savedPin,sizeof(savedPin))==sizeof(savedPin)&&!memcmp(savedPin.magic,"T5PIN001",8)&&pinHash(pin,master,savedPin.salt,digest);
 uint8_t difference=0;for(unsigned j=0;j<32;++j)difference|=digest[j]^savedPin.digest[j];valid=valid&&!difference;
 tab5vault::wipe(digest,sizeof(digest));tab5vault::wipe(&savedPin,sizeof(savedPin));
 if(!valid){message="PIN incorrect";pinFailures=std::min(10u,pinFailures+1);if(pinFailures>=3)pinRetryAt=millis()+std::min(300000u,1000u<<(pinFailures-3));}
 else{pinFailures=0;pinRetryAt=0;}return valid;
}
const char* decrypt(Protected& data,Secrets& keys){Memory in(data.cipher,sizeof(data.cipher),true),out(&keys.ring,sizeof(keys.ring),false);const char* e=tab5vault::decryptKey(in,out,keys.master,data.id);if(e)return "Mot de passe incorrect ou coffre altéré";return out.length==RingBytes&&validRing(keys.ring)?nullptr:"Format du coffre invalide";}
const char* persist(Protected& data,Secrets& keys,bool local=false){
 uint8_t entropy[24];if(!Tab5CryptoRandom::fill(entropy,sizeof(entropy)))return "Aléa cryptographique indisponible";Memory in(&keys.ring,sizeof(keys.ring),true),out(data.cipher,sizeof(data.cipher),false);
 const char* error=tab5vault::encryptKey(in,out,keys.master,data.id,entropy);if(error)return error;
 Preferences p;if(!p.begin("tab5keys",false))return "Mémoire des clés inaccessible";
 bool ok=false;
 if(local){Scratch<DeviceEnvelope> savedMemory,checkMemory;if(!savedMemory||!checkMemory){p.end();return "Mémoire insuffisante";}auto& saved=*savedMemory;auto& check=*checkMemory;saved.data=data;memcpy(saved.master,keys.master,32);ok=p.putBytes("device3",&saved,sizeof(saved))==sizeof(saved);ok=ok&&p.getBytesLength("device3")==sizeof(check)&&p.getBytes("device3",&check,sizeof(check))==sizeof(check)&&!memcmp(&saved,&check,sizeof(saved));}
 else {Scratch<Protected> checkMemory;if(!checkMemory){p.end();return "Mémoire insuffisante";}auto& check=*checkMemory;ok=p.putBytes("vault2",&data,sizeof(data))==sizeof(data);ok=ok&&p.getBytesLength("vault2")==sizeof(check)&&p.getBytes("vault2",&check,sizeof(check))==sizeof(check)&&!memcmp(&check,&data,sizeof(data));}
 p.end();
 return ok?nullptr:"Écriture de la clé impossible";
}
void unlockTask(void*){
 uint32_t token=jobGeneration;Secrets next;Protected data;const char* error=nullptr;bool exists=false;
 {Guard guard;Preferences p;if(!p.begin("tab5keys",false))error="Mémoire des clés inaccessible";
  else {exists=p.isKey("vault2");
   if(exists&&!changing){if(p.getBytesLength("vault2")!=sizeof(data)||p.getBytes("vault2",&data,sizeof(data))!=sizeof(data))error="Format du coffre invalide";}
   else if(changing){if(!isOpen)error="Coffre verrouillé";else next.ring=session.ring;}
   else {for(unsigned i=0;i<Count&&!error;++i){char name[8];slotName(i,name);if(!p.isKey(name))continue;if(p.getBytesLength(name)!=sizeof(Record)||p.getBytes(name,&next.ring.keys[i],sizeof(Record))!=sizeof(Record)||!valid(next.ring.keys[i]))error="Sauvegarde de clé invalide";else next.ring.used[i]=1;}}
   p.end();}}
 if(!error){if(!exists||changing){if(!Tab5CryptoRandom::fill(data.salt,sizeof(data.salt))||!Tab5CryptoRandom::fill(data.id,sizeof(data.id)))error="Aléa cryptographique indisponible";}
  auto tick=[](size_t,size_t,void* context){vTaskDelay(1);return generation.load()==*static_cast<uint32_t*>(context);};
  if(!error&&!tab5vault::passwordKey(password,data.salt,next.master,tick,&token))error="Échec de dérivation du mot de passe";
 }
 tab5vault::wipe(password,sizeof(password));
 if(!error&&exists&&!changing)error=decrypt(data,next);
 {Guard guard;
  if(generation.load()!=token)error="Coffre verrouillé";
  if(!error&&(!exists||changing))error=persist(data,next);
  if(!error){session=next;envelope=data;deviceMode=false;isOpen=true;lastUse=millis();
   // Logical deletion preserves migration through power loss. Flash wear
   // levelling can retain old plaintext pages; this is documented explicitly.
   Preferences p;if(p.begin("tab5keys",false)){for(unsigned i=0;i<Count;++i){char name[8];slotName(i,name);p.remove(name);}p.end();}
  }
  message=error?error:changing?"Mot de passe du coffre modifié":"Coffre déverrouillé";
 }
 tab5vault::wipe(&next,sizeof(next));working=false;tab5TaskCheckpoint("vault unlock");vTaskDelete(nullptr);
}
}
namespace Tab5KeyStore {
bool configured(){Guard guard;return stored()||stored("device3");}
bool deviceConfigured(){Guard guard;return stored("device3");}
bool legacyAvailable(){Guard guard;return stored();}
bool usingDevice(){StatusGuard guard;return guard.held&&!working&&authorized()&&deviceMode;}
bool credentialsAccessible(){StatusGuard guard;return guard.held&&!working&&isOpen&&deviceMode&&sessionPinProtected;}
// The device mode intentionally stores its random master in internal NVS.
// It protects a removed SD card, not somebody holding this unlocked Tab5.
// One verified NVS blob avoids separating a new key from its encrypted ring.
static bool openDeviceImpl(bool migrate,const char* pin,bool initialize=false,bool worker=false){
 Guard guard;if((working&&!worker)||Tab5Vault::busy()){message="Opération en cours : patienter ou annuler";return false;}
 if(worker&&generation.load()!=jobGeneration){message="Coffre verrouillé";return false;}
 bool checkPin=false,hasPin=false;
 if(stored("reset4")){message="Réinitialisation du coffre à terminer";return false;}
 if(initialize&&!validPin(pin)){message="PIN : 4 à 12 chiffres";return false;}
 if(migrate&&(!isOpen||deviceMode)){message="Ouvrez d’abord l’ancien coffre";return false;}
 Scratch<Secrets> nextMemory;Scratch<Protected> dataMemory;if(!nextMemory||!dataMemory){message="Mémoire insuffisante";return false;}auto& next=*nextMemory;auto& data=*dataMemory;const char* error=nullptr;bool exists=false;
 if(worker)tab5DiagnosticMark(6,"nvs-read","",1);
 {Preferences p;if(!p.begin("tab5keys",false))error="Mémoire des clés inaccessible";
  else {hasPin=p.isKey("pin4");if(!hasPin&&!initialize){p.end();message="Créer un PIN pour le coffre";return false;}checkPin=hasPin&&!(isOpen&&deviceMode);if(checkPin&&!pin){p.end();message="Saisir le PIN du coffre";return false;}if(checkPin&&pinRetryAt&&int32_t(pinRetryAt-millis())>0){p.end();message="Trop de tentatives : patienter";return false;}
   exists=p.isKey("device3");if(exists){Scratch<DeviceEnvelope> savedMemory;if(!savedMemory){p.end();message="Mémoire insuffisante";return false;}auto& saved=*savedMemory;if(p.getBytesLength("device3")!=sizeof(saved)||p.getBytes("device3",&saved,sizeof(saved))!=sizeof(saved)||memcmp(saved.magic,"T5DEV001",8))error="Format du coffre appareil invalide";else{data=saved.data;memcpy(next.master,saved.master,32);
    if(worker)tab5DiagnosticMark(6,"pin-auth","",1);
    if(checkPin&&!verifyPin(p,pin,next.master))error=message.load();
    if(!error){if(worker)tab5DiagnosticMark(6,"decrypt","",1);if(decrypt(data,next))error="Coffre appareil illisible - ancien coffre conservé";}
   }}else if(checkPin)error="Format du coffre appareil invalide";p.end();}}
 if(!error&&!exists&&!initialize)error="Créer un PIN pour le coffre";
 if(!error&&!exists){if(!Tab5CryptoRandom::fill(next.master,32)||!Tab5CryptoRandom::fill(data.id,16)||!Tab5CryptoRandom::fill(data.salt,16))error="Aléa cryptographique indisponible";}
 if(!error&&migrate){
  for(unsigned i=0;i<Count&&!error;++i){if(!session.ring.used[i])continue;int free=-1,found=-1;
   for(unsigned j=0;j<Count;++j){if(!next.ring.used[j]){if(free<0)free=j;}else if(!memcmp(session.ring.keys[i].id,next.ring.keys[j].id,16))found=j;}
   if(found>=0){if(memcmp(session.ring.keys[i].key,next.ring.keys[found].key,32))error="Identifiant de clé déjà utilisé";}
   else if(free<0)error="Limite de 8 clés atteinte — ancien coffre conservé";
   else {next.ring.keys[free]=session.ring.keys[i];next.ring.used[free]=1;}
  }
 }
 if(!error&&(!exists||migrate))error=persist(data,next,true);
 if(!error){generation.fetch_add(1);session=next;envelope=data;deviceMode=true;sessionPinProtected=hasPin;isOpen=true;lastUse=millis();pinFailures=0;pinRetryAt=0;}
 if(error||!worker)message=error?error:migrate?"Clés copiées - ancien coffre conservé":checkPin?"Coffre déverrouillé":"Coffre appareil ouvert sans code";return !error;
}
bool openDevice(bool migrate){return openDeviceImpl(migrate,nullptr);}
bool unlockDevice(const char* pin,bool migrate){return openDeviceImpl(migrate,pin);}
bool pinConfigured(){Guard guard;return stored("pin4");}
unsigned pinRetrySeconds(){Guard guard;int32_t remaining=int32_t(pinRetryAt-millis());return pinRetryAt&&remaining>0?unsigned(remaining+999)/1000:0;}
static bool configurePinImpl(const char* pin,const char* current,bool worker=false){
 Guard guard;if(!isOpen||!deviceMode){message="Ouvrir le coffre appareil";return false;}if((working&&!worker)||Tab5Vault::busy()){message="Opération en cours : patienter ou annuler";return false;}
 if(!pin||(*pin&&!validPin(pin))){message="PIN : 4 à 12 chiffres";return false;}
 Preferences p;if(!p.begin("tab5keys",false)){message="Mémoire des clés inaccessible";return false;}bool ok=false;
 if(p.isKey("pin4")&&!verifyPin(p,current,session.master)){p.end();return false;}
 if(!p.isKey("pin4")&&!*pin){p.end();message="Créer un PIN pour le coffre";return false;}
 if(!*pin)ok=!p.isKey("pin4")||p.remove("pin4");
 else{PinRecord next,check;ok=Tab5CryptoRandom::fill(next.salt,sizeof(next.salt))&&pinHash(pin,session.master,next.salt,next.digest);if(ok)ok=p.putBytes("pin4",&next,sizeof(next))==sizeof(next)&&p.getBytesLength("pin4")==sizeof(check)&&p.getBytes("pin4",&check,sizeof(check))==sizeof(check)&&!memcmp(&check,&next,sizeof(next));tab5vault::wipe(&next,sizeof(next));tab5vault::wipe(&check,sizeof(check));}
 p.end();if(ok){sessionPinProtected=*pin;if(!*pin)generation.fetch_add(1);}message=ok?"Accès du coffre modifié":"Écriture de la clé impossible";return ok;
}
bool configurePin(const char* pin,const char* current){return configurePinImpl(pin,current);}
bool initializeDevice(const char* pin,bool migrate){
 if(!openDeviceImpl(migrate,pin,true))return false;
 if(pinConfigured())return true;
 if(configurePinImpl(pin,nullptr))return true;
 const char* error=message.load();lock();message=error;return false;
}
bool resetPending(){Guard guard;return stored("reset4");}
bool operationSucceeded(){return operationOk.load();}
static bool resetDeviceImpl(){
 // A durable tombstone keeps access closed during interrupted recovery. The
 // directory rename preserves every encrypted credential bank. Only device3
 // and pin4 are erased; legacy password stores and all user files remain.
 ResetRecord reset;
 tab5DiagnosticMark(6,"reset-prepare","",1);
 {Guard guard;generation.fetch_add(1);clearSession();Preferences p;
  if(!p.begin("tab5keys",false)){message="Mémoire des clés inaccessible";return false;}
  bool ok=true;
  if(p.isKey("reset4"))ok=p.getBytesLength("reset4")==sizeof(reset)&&p.getBytes("reset4",&reset,sizeof(reset))==sizeof(reset)&&validReset(reset);
  else{uint8_t random[8];ok=Tab5CryptoRandom::fill(random,sizeof(random));if(ok){strcpy(reset.archive,"/System/Secrets-retired-");for(unsigned i=0;i<8;++i)snprintf(reset.archive+24+i*2,3,"%02x",random[i]);ResetRecord check;ok=p.putBytes("reset4",&reset,sizeof(reset))==sizeof(reset)&&p.getBytesLength("reset4")==sizeof(check)&&p.getBytes("reset4",&check,sizeof(check))==sizeof(check)&&!memcmp(&reset,&check,sizeof(reset));}}
  p.end();if(!ok){message="Préparation de la récupération impossible";return false;}
 }
 tab5DiagnosticMark(6,"reset-archive","",1);
 if(!Tab5Secrets::retireStore(reset.archive)){message=Tab5Secrets::status();return false;}
 tab5DiagnosticMark(6,"reset-commit","",1);
 {Guard guard;Preferences p;if(!p.begin("tab5keys",false)){message="Mémoire des clés inaccessible";return false;}
  bool ok=(!p.isKey("device3")||p.remove("device3"))&&(!p.isKey("pin4")||p.remove("pin4"))&&p.remove("reset4");p.end();
  if(!ok){message="Réinitialisation du coffre à terminer";return false;}deviceMode=false;pinFailures=0;pinRetryAt=0;message="Coffre réinitialisé ; fichiers chiffrés conservés";
 }return true;
}
static void deviceTask(void*){
 bool ok=false;const auto task=deviceJob;const uint32_t token=jobGeneration;
 tab5DiagnosticMark(6,"auth","",1);
 if(generation.load()==token){
  if(task==DeviceJob::Reset)ok=resetDeviceImpl();
  else if(task==DeviceJob::ChangePin)ok=configurePinImpl(requestedPin,currentPin,true);
  else{
   ok=openDeviceImpl(requestedMigration,requestedPin,task==DeviceJob::Initialize,true);
   if(ok&&task==DeviceJob::Initialize&&!pinConfigured())ok=configurePinImpl(requestedPin,nullptr,true);
   if(!ok&&task==DeviceJob::Initialize){const char* error=message.load();lock();message=error;}
   // Loading both encrypted banks is intentionally off the UI callback stack.
   if(ok){
    message="Lecture des identifiants…";
    tab5DiagnosticMark(6,"preload","",1);
    bool loaded=Tab5Secrets::preload();uint32_t revision=0;
    ok=sessionToken(revision);
    message=!ok?"Coffre verrouillé":!loaded?Tab5Secrets::status():"Coffre déverrouillé";
   }
  }
 }
 tab5vault::wipe(requestedPin,sizeof(requestedPin));tab5vault::wipe(currentPin,sizeof(currentPin));
 tab5TaskCheckpoint("vault device");
 operationOk=ok;
 {Guard guard;working=false;bool handoff=ok&&uiContinuation.load();tab5DiagnosticMark(6,handoff?"await-ui":ok?"ready":"failed","",handoff?1:0);}
 vTaskDelete(nullptr);
}
static bool startDeviceJob(DeviceJob job,const char* pin,const char* current,bool migrate,bool ui){
 Guard guard;if(working||Tab5Vault::busy()){message="Opération en cours : patienter ou annuler";return false;}
 if((pin&&strlen(pin)>12)||(current&&strlen(current)>12)){message="PIN : 4 à 12 chiffres";return false;}
 deviceJob=job;requestedMigration=migrate;strcpy(requestedPin,pin?pin:"");strcpy(currentPin,current?current:"");jobGeneration=generation.load();operationOk=false;working=true;uiContinuation=ui;message="Ouverture du coffre.";tab5DiagnosticMark(6,"unlock","",1);
 if(tab5CreateTask(deviceTask,"vault_device",16384,nullptr,1,nullptr,0)!=pdPASS){working=false;uiContinuation=false;tab5vault::wipe(requestedPin,sizeof(requestedPin));tab5vault::wipe(currentPin,sizeof(currentPin));message="Mémoire insuffisante";tab5DiagnosticMark(6,"failed","",0);return false;}return true;
}
bool beginDeviceUnlock(const char* pin,bool initialize,bool migrate,bool ui){return startDeviceJob(initialize?DeviceJob::Initialize:DeviceJob::Open,pin,nullptr,migrate,ui);}
bool beginPinChange(const char* current,const char* next,bool ui){return startDeviceJob(DeviceJob::ChangePin,next,current,false,ui);}
bool beginReset(bool ui){return startDeviceJob(DeviceJob::Reset,nullptr,nullptr,false,ui);}
void finishUi(){Guard guard;uiContinuation=false;if(!working)tab5DiagnosticMark(6,"ready","",0);}
bool deviceKey(uint8_t key[32],uint8_t id[16],uint32_t* revision){Guard guard;if(!key||!id)return false;if(!isOpen||!deviceMode||!sessionPinProtected){tab5vault::wipe(key,32);tab5vault::wipe(id,16);message="Ouvrir le coffre appareil";return false;}memcpy(key,session.master,32);memcpy(id,envelope.id,16);if(revision)*revision=generation.load();lastUse=millis();return true;}
bool sessionCurrent(uint32_t revision){return isOpen.load()&&generation.load()==revision;}
bool sessionToken(uint32_t& revision){Guard guard;if(!authorized())return false;revision=generation.load();return true;}
bool unlocked(){StatusGuard guard;return guard.held&&!working&&authorized();}
bool busy(){return working.load();}
bool unlock(const char* pass,bool change){
 if(!pass||strlen(pass)>256||working)return false;
 unsigned count=0;for(const unsigned char* p=reinterpret_cast<const unsigned char*>(pass);*p;++p)if((*p&0xc0)!=0x80)++count;
 if(count<12){message="Mot de passe : au moins 12 caractères";return false;}
 Guard guard;if(working)return false;
 if(change&&!isOpen){message="Coffre verrouillé";return false;}
 strcpy(password,pass);changing=change;jobGeneration=generation.load();working=true;message="Ouverture du coffre…";
 if(tab5CreateTask(unlockTask,"vault_unlock",16384,nullptr,0,nullptr,0)!=pdPASS){working=false;tab5vault::wipe(password,sizeof(password));message="Mémoire insuffisante";return false;}return true;
}
void lock(){{Guard guard;generation.fetch_add(1);clearSession();Tab5Vault::cancel();message="Coffre verrouillé";}Tab5Secrets::clearCache();}
void service(bool off){if(off&&(isOpen||working))lock();}
bool read(unsigned slot,Record& r){Guard guard;if(!authorized()||slot>=Count||!session.ring.used[slot]){tab5vault::wipe(&r,sizeof(r));message=isOpen?"Clé absente":"Coffre verrouillé";return false;}r=session.ring.keys[slot];lastUse=millis();return true;}
std::vector<Info> list(){Guard guard;std::vector<Info> out;if(!authorized())return out;for(unsigned i=0;i<Count;++i)if(session.ring.used[i]){auto& r=session.ring.keys[i];char id[33];for(unsigned j=0;j<16;++j)snprintf(id+j*2,3,"%02x",r.id[j]);out.push_back({i,r.name,id});}return out;}
int find(const uint8_t id[16]){Guard guard;if(!authorized())return -1;for(unsigned i=0;i<Count;++i)if(session.ring.used[i]&&!memcmp(session.ring.keys[i].id,id,16))return i;return -1;}
int import(const Record& r){Guard guard;if(!authorized()||working){message="Coffre verrouillé";return -1;}if(!valid(r)){message="Sauvegarde de clé invalide";return -1;}
 int slot=-1;for(unsigned i=0;i<Count;++i){if(!session.ring.used[i]){if(slot<0)slot=i;continue;}auto& old=session.ring.keys[i];if(!memcmp(old.id,r.id,16)){bool same=!memcmp(old.key,r.key,32);message=same?"Clé déjà présente":"Identifiant de clé déjà utilisé";return same?int(i):-1;}}
 if(slot<0){message="Limite de 8 clés atteinte";return -1;}
 Scratch<Secrets> nextMemory;Scratch<Protected> dataMemory;if(!nextMemory||!dataMemory){message="Mémoire insuffisante";return -1;}auto& next=*nextMemory;auto& data=*dataMemory;memcpy(next.master,session.master,32);next.ring=session.ring;next.ring.keys[slot]=r;next.ring.used[slot]=1;data=envelope;
 const char* error=persist(data,next,deviceMode);if(!error){session=next;envelope=data;lastUse=millis();}
 message=error?error:"Clé enregistrée dans le Tab5";return error?-1:slot;
}
int create(const char* name){if(!name||!*name||strlen(name)>48||strchr(name,'\n')){message="Nom de clé : 1 à 48 octets";return -1;}Record r;if(!Tab5CryptoRandom::fill(r.id,sizeof(r.id))||!Tab5CryptoRandom::fill(r.key,sizeof(r.key))){tab5vault::wipe(&r,sizeof(r));message="Aléa cryptographique indisponible";return -1;}strcpy(r.name,name);int result=import(r);tab5vault::wipe(&r,sizeof(r));return result;}
bool erase(unsigned slot){Guard guard;if(!authorized()){message="Coffre verrouillé";return false;}if(working||Tab5Vault::busy()){message="Opération en cours : patienter ou annuler";return false;}if(slot>=Count||!session.ring.used[slot]){message="Clé absente";return false;}
 Scratch<Secrets> nextMemory;Scratch<Protected> dataMemory;if(!nextMemory||!dataMemory){message="Mémoire insuffisante";return false;}auto& next=*nextMemory;auto& data=*dataMemory;memcpy(next.master,session.master,32);next.ring=session.ring;tab5vault::wipe(&next.ring.keys[slot],sizeof(Record));next.ring.used[slot]=0;data=envelope;const char* error=persist(data,next,deviceMode);if(!error){session=next;envelope=data;lastUse=millis();}message=error?error:"Clé supprimée";return !error;
}
const char* status(){return message.load();}
}
