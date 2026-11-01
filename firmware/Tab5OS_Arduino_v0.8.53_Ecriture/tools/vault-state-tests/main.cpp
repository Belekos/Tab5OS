// Host state-machine tests: cipher/NVS/task APIs are doubles, not a cryptographic test.
#include "Tab5KeyStore.h"
#include "Tab5VaultCore.h"
#include "Preferences.h"
#include <cassert>
#include <cstdio>
#include <vector>
std::map<std::string,std::vector<unsigned char>> Preferences::values;bool Preferences::failWrite=false;
namespace tab5vault {
void wipe(void* p,size_t n){memset(p,0,n);}
bool passwordKey(const char* p,const uint8_t* salt,uint8_t* out){for(unsigned i=0;i<32;++i)out[i]=salt[i%16]^p[i%strlen(p)]^i;return true;}
const char* encryptKey(Stream& in,Stream& out,const uint8_t* key,const uint8_t* id,const uint8_t*,Progress,void*){uint8_t h[64]{};memcpy(h,key,32);memcpy(h+32,id,16);uint32_t n=in.size();memcpy(h+48,&n,4);if(out.write(h,64)!=64)return "write";std::vector<uint8_t> b(n);in.read(b.data(),n);out.write(b.data(),n);uint8_t tag[16]{};out.write(tag,16);return nullptr;}
const char* decryptKey(Stream& in,Stream& out,const uint8_t* key,const uint8_t* id,Progress,void*){uint8_t h[64];if(in.read(h,64)!=64||memcmp(h,key,32)||memcmp(h+32,id,16))return "wrong key";uint32_t n;memcpy(&n,h+48,4);if(n>2000)return "format";std::vector<uint8_t> b(n);if(in.read(b.data(),n)!=n||out.write(b.data(),n)!=n)return "io";return nullptr;}
}
void reset(){Tab5KeyStore::lock();Preferences::failWrite=false;Preferences::values.clear();}
int main(){using namespace Tab5KeyStore;
reset();assert(!configured());assert(openDevice());assert(usingDevice());int first=create("Personnel");assert(first==0);Record key;assert(read(first,key));lock();assert(!unlocked());assert(openDevice());Record again;assert(read(first,again));assert(!memcmp(key.key,again.key,32));
reset();assert(unlock("legacy-pass-123"));int legacy=create("Old");assert(legacy==0);Record old;assert(read(legacy,old));auto original=Preferences::values["tab5keys/vault2"];lock();assert(openDevice());assert(list().empty());assert(create("New")==0);lock();assert(unlock("legacy-pass-123"));assert(openDevice(true));assert(list().size()==2);assert(find(old.id)>=0);assert(Preferences::values["tab5keys/vault2"]==original);lock();assert(openDevice());assert(find(old.id)>=0);lock();assert(unlock("wrong-password-123"));assert(!unlocked());assert(unlock("legacy-pass-123"));assert(find(old.id)>=0);
lock();assert(openDevice());while(list().size()<MaxKeys)assert(create("Extra")>=0);auto full=Preferences::values["tab5keys/device3"];lock();assert(unlock("legacy-pass-123"));assert(create("Another old")>=0);assert(!openDevice(true));assert(!usingDevice());assert(Preferences::values["tab5keys/device3"]==full);
reset();assert(unlock("legacy-pass-123"));assert(create("Safe")>=0);original=Preferences::values["tab5keys/vault2"];Preferences::failWrite=true;assert(!openDevice(true));assert(unlocked()&&!usingDevice());assert(list().size()==1);assert(Preferences::values["tab5keys/vault2"]==original);assert(!deviceConfigured());Preferences::failWrite=false;assert(openDevice(true));lock();Preferences::values["tab5keys/device3"][0]^=1;auto corrupt=Preferences::values["tab5keys/device3"];assert(!openDevice());assert(!unlocked());assert(Preferences::values["tab5keys/device3"]==corrupt);
puts("PASS: device reopen, legacy preservation, independent vault, merge, capacity refusal, failed write rollback, corrupt blob refusal");}
