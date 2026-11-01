#include "Tab5Secrets.h"
#include "Tab5KeyStore.h"
#include "Tab5VaultCore.h"
#include "Tab5CryptoRandom.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mbedtls/md.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif
namespace {
constexpr const char* Folder="/System/Secrets";
constexpr const char* Banks[]={"/System/Secrets/credentials.a.t5v","/System/Secrets/credentials.b.t5v"};
struct Entry{char name[Tab5Secrets::MaxNameBytes+1]{},value[Tab5Secrets::MaxValueBytes+1]{};};
struct Table{uint8_t magic[8]={'T','5','S','E','C','0','0','1'};uint32_t serial=0;uint8_t used[Tab5Secrets::MaxNames]{};Entry entries[Tab5Secrets::MaxNames];};
struct TablePtr{
 Table* value=nullptr;
 TablePtr(){
#ifdef ARDUINO
  void* memory=heap_caps_malloc(sizeof(Table),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
  void* memory=malloc(sizeof(Table));
#endif
  if(memory)value=new(memory)Table;
 }
 ~TablePtr(){reset();}
 void reset(){if(value){tab5vault::wipe(value,sizeof(Table));
#ifdef ARDUINO
  heap_caps_free(value);
#else
  free(value);
#endif
  value=nullptr;}}
 Table* release(){auto* out=value;value=nullptr;return out;}
};
Table* cache=nullptr;uint32_t cacheRevision=0;int activeBank=-1;
bool loadAttempted=false;const char* loadError=nullptr;
StaticSemaphore_t mutexStorage;SemaphoreHandle_t mutex(){static auto m=xSemaphoreCreateMutexStatic(&mutexStorage);return m;}
struct Guard{Guard(){xSemaphoreTake(mutex(),portMAX_DELAY);}~Guard(){xSemaphoreGive(mutex());}};
struct StatusGuard{bool held=xSemaphoreTake(mutex(),0);~StatusGuard(){if(held)xSemaphoreGive(mutex());}};
std::atomic<const char*> message{"Coffre verrouillé"};
struct Key{uint8_t bytes[32]{},id[16]{};uint32_t revision=0;~Key(){tab5vault::wipe(this,sizeof(*this));}};
bool key(Key& out){uint8_t master[32]{};if(!Tab5KeyStore::deviceKey(master,out.id,&out.revision)){message="Ouvrir le coffre appareil";return false;}
 static const uint8_t domain[]="Tab5 credential store v1";bool ok=mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),master,32,domain,sizeof(domain)-1,out.bytes)==0;tab5vault::wipe(master,sizeof(master));if(!ok)message="Chiffrement indisponible";return ok;}
bool validName(const char* name){if(!name||!*name||strlen(name)>Tab5Secrets::MaxNameBytes)return false;auto alpha=[](unsigned char c){return(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';};if(!alpha(*name))return false;for(++name;*name;++name)if(!alpha(*name)&&(*name<'0'||*name>'9'))return false;return true;}
bool valid(const Table& t){if(memcmp(t.magic,"T5SEC001",8))return false;for(unsigned i=0;i<Tab5Secrets::MaxNames;++i){if(t.used[i]>1)return false;if(!t.used[i])continue;const auto& e=t.entries[i];if(!memchr(e.name,0,sizeof(e.name))||!memchr(e.value,0,sizeof(e.value))||!validName(e.name))return false;for(unsigned j=0;j<i;++j)if(t.used[j]&&!strcmp(e.name,t.entries[j].name))return false;}return true;}
struct Memory:tab5vault::Stream{uint8_t* data;size_t length,position=0;Memory(Table& t,bool input):data(reinterpret_cast<uint8_t*>(&t)),length(input?sizeof(t):0){}size_t read(void* p,size_t n)override{n=std::min(n,length-position);memcpy(p,data+position,n);position+=n;return n;}size_t write(const void* p,size_t n)override{if(n>sizeof(Table)-position)return 0;memcpy(data+position,p,n);position+=n;length=std::max(length,position);return n;}bool seek(size_t n)override{if(n>length)return false;position=n;return true;}size_t size()override{return length;}};
struct FileStream:tab5vault::Stream{File file;explicit FileStream(const char* path,bool write=false):file(tab5FS.open(path,write?FILE_WRITE:FILE_READ)){}size_t read(void* p,size_t n)override{return file.read(static_cast<uint8_t*>(p),n);}size_t write(const void* p,size_t n)override{return file.write(static_cast<const uint8_t*>(p),n);}bool seek(size_t n)override{return file.seek(n);}size_t size()override{return file.size();}};
void clear(){if(cache){tab5vault::wipe(cache,sizeof(Table));
#ifdef ARDUINO
 heap_caps_free(cache);
#else
 free(cache);
#endif
 cache=nullptr;}cacheRevision=0;activeBank=-1;loadAttempted=false;loadError=nullptr;}
bool readBank(const char* path,Table& table,const Key& k){FileStream in(path);if(!in.file||in.file.isDirectory())return false;constexpr size_t expected=tab5vault::KeyHeaderSize+sizeof(Table)+16*((sizeof(Table)+tab5vault::ChunkSize-1)/tab5vault::ChunkSize);if(in.size()!=expected)return false;Memory out(table,false);
 auto progress=[](size_t,size_t,void* context){vTaskDelay(1);return Tab5KeyStore::sessionCurrent(static_cast<const Key*>(context)->revision);};
 return !tab5vault::decryptKey(in,out,k.bytes,k.id,progress,const_cast<Key*>(&k))&&out.length==sizeof(Table)&&valid(table);}
bool load(const Key& k){
 // The worker preloads this session before the UI may open its page. A failed
 // load is also a result: do not silently retry SD/decryption from names/get
 // on the UI task. Lock/unlock or an explicit clearCache starts a fresh attempt.
 if(loadAttempted&&cacheRevision==k.revision){if(!cache)message=loadError;return cache!=nullptr;}
 clear();cacheRevision=k.revision;loadAttempted=true;
 auto failed=[](){loadError=message.load();return false;};
 Tab5StorageUse lease;if(!lease){message="Reconnexion SD en cours - réessayez";return failed();}if(!tab5SdAvailable()){message="Insérer la carte SD";return failed();}TablePtr next,candidate;if(!next.value||!candidate.value){message="Mémoire insuffisante";return failed();}
 bool any=false;int chosen=-1;
 for(int i=0;i<2;++i){
  // Only ENOENT proves a bank is absent. An I/O failure must not create an
  // empty store or silently select the older bank and overwrite newer data.
  errno=0;bool present=tab5FS.exists(Banks[i]);
  if(!present){if(errno==ENOENT)continue;message="Coffre de mots de passe illisible";return failed();}
  any=true;errno=0;bool readable=readBank(Banks[i],*candidate.value,k);
  if(!readable&&errno){message="Coffre de mots de passe illisible";return failed();}
  if(readable&&(chosen<0||int32_t(candidate.value->serial-next.value->serial)>0)){*next.value=*candidate.value;chosen=i;}
 }
 if(any&&chosen<0){message="Coffre de mots de passe illisible";return failed();}if(!Tab5KeyStore::sessionCurrent(k.revision)){message="Coffre verrouillé";return failed();}cache=next.release();activeBank=chosen;return true;}
int index(const char* name){for(unsigned i=0;i<Tab5Secrets::MaxNames;++i)if(cache->used[i]&&!strcmp(cache->entries[i].name,name))return int(i);return -1;}
bool save(Table& next,const Key& k){Tab5StorageUse lease;if(!lease){message="Reconnexion SD en cours - réessayez";return false;}
 if(!tab5FS.exists("/System")&&!tab5FS.mkdir("/System")){message="Écriture du coffre impossible";return false;}if(!tab5FS.exists(Folder)&&!tab5FS.mkdir(Folder)){message="Écriture du coffre impossible";return false;}
 const int target=activeBank==0?1:0;String temp=String(Banks[target])+".tmp";if(tab5FS.exists(temp)&&!tab5FS.remove(temp)){message="Écriture du coffre impossible";return false;}
 uint8_t entropy[24];if(!Tab5CryptoRandom::fill(entropy,sizeof(entropy))){message="Aléa cryptographique indisponible";return false;}next.serial=cache->serial+1;const char* error=nullptr;
 {FileStream out(temp.c_str(),true);Memory input(next,true);if(!out.file)error="Écriture du coffre impossible";else{error=tab5vault::encryptKey(input,out,k.bytes,k.id,entropy);errno=0;out.file.flush();if(!error&&errno)error="Écriture du coffre impossible";errno=0;out.file.close();if(!error&&errno)error="Écriture du coffre impossible";}}
 // Verify the closed file before touching either committed bank. The old
 // active bank survives every failure, removal, and rename interruption.
 TablePtr verified;if(!error&&(!verified.value||!readBank(temp.c_str(),*verified.value,k)||memcmp(&next,verified.value,sizeof(next))))error="Vérification du coffre impossible";
 if(!error&&!Tab5KeyStore::sessionCurrent(k.revision))error="Coffre verrouillé";
 if(!error&&tab5FS.exists(Banks[target])&&!tab5FS.remove(Banks[target]))error="Écriture du coffre impossible";
 if(!error&&!tab5FS.rename(temp,Banks[target]))error="Écriture du coffre impossible";
 if(error){tab5FS.remove(temp);message=error;return false;}*cache=next;activeBank=target;message="Secret enregistré";return true;}
}
namespace Tab5Secrets {
bool available(){return !Tab5KeyStore::busy()&&Tab5KeyStore::credentialsAccessible();}
bool preload(){Guard guard;Key k;return key(k)&&load(k);}
bool ready(){StatusGuard guard;return guard.held&&cache&&Tab5KeyStore::sessionCurrent(cacheRevision);}
void clearCache(){Guard guard;clear();}
bool retireStore(const char* destination){
 Guard guard;clear();Tab5StorageUse lease;
 if(!lease){message="Reconnexion SD en cours - réessayez";return false;}
 if(!tab5SdAvailable()){message="Insérer la carte SD pour récupérer le coffre";return false;}
 if(!destination||strncmp(destination,"/System/Secrets-retired-",24)){message="Chemin de récupération invalide";return false;}
 errno=0;bool present=tab5FS.exists(Folder);
 if(!present){if(errno==ENOENT)return true;message="Archivage du coffre impossible";return false;}
 errno=0;bool occupied=tab5FS.exists(destination);
 if(occupied||errno!=ENOENT||!tab5FS.rename(Folder,destination)){message="Archivage du coffre impossible";return false;}
 return true;
}
bool get(const char* name,char* output,size_t capacity){if(output&&capacity)tab5vault::wipe(output,capacity);if(!output||!capacity||Tab5KeyStore::busy())return false;Guard guard;Key k;if(!key(k)||!load(k))return false;if(!validName(name)){message="Nom de variable invalide";return false;}int at=index(name);if(at<0){message="Secret absent";return false;}size_t size=strlen(cache->entries[at].value);if(size>=capacity){message="Valeur trop longue";return false;}if(!Tab5KeyStore::sessionCurrent(k.revision)){clear();message="Coffre verrouillé";return false;}memcpy(output,cache->entries[at].value,size+1);message="Secret disponible";return true;}
bool set(const char* name,const char* value){if(Tab5KeyStore::busy())return false;Guard guard;if(!validName(name)){message="Nom de variable invalide";return false;}if(!value||strlen(value)>MaxValueBytes){message="Valeur trop longue";return false;}Key k;if(!key(k)||!load(k))return false;int at=index(name);if(at<0)for(unsigned i=0;i<MaxNames;++i)if(!cache->used[i]){at=int(i);break;}if(at<0){message="Limite de 64 secrets atteinte";return false;}if(cache->used[at]&&!strcmp(cache->entries[at].value,value)){message="Secret enregistré";return true;}TablePtr next;if(!next.value){message="Mémoire insuffisante";return false;}*next.value=*cache;tab5vault::wipe(&next.value->entries[at],sizeof(Entry));strcpy(next.value->entries[at].name,name);strcpy(next.value->entries[at].value,value);next.value->used[at]=1;return save(*next.value,k);}
bool erase(const char* name){if(Tab5KeyStore::busy())return false;Guard guard;Key k;if(!key(k)||!load(k))return false;if(!validName(name)){message="Nom de variable invalide";return false;}int at=index(name);if(at<0){message="Secret absent";return false;}TablePtr next;if(!next.value){message="Mémoire insuffisante";return false;}*next.value=*cache;tab5vault::wipe(&next.value->entries[at],sizeof(Entry));next.value->used[at]=0;bool ok=save(*next.value,k);if(ok)message="Secret supprimé";return ok;}
std::vector<String> names(){if(Tab5KeyStore::busy())return {};Guard guard;std::vector<String> result;Key k;if(!key(k)||!load(k))return result;for(unsigned i=0;i<MaxNames;++i)if(cache->used[i])result.push_back(cache->entries[i].name);std::sort(result.begin(),result.end(),[](const String& a,const String& b){return strcmp(a.c_str(),b.c_str())<0;});return result;}
const char* status(){return message.load();}
}
