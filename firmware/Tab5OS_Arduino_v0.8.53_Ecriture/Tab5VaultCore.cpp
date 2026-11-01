#include "Tab5VaultCore.h"
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/version.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#include <sdkconfig.h>
#endif
namespace tab5vault {
void wipe(void* data,size_t n){volatile uint8_t* p=static_cast<volatile uint8_t*>(data);while(n--)*p++=0;}
namespace {
void put32(uint8_t* p,uint32_t v){for(int i=0;i<4;++i)p[i]=v>>(8*i);}
uint32_t get32(const uint8_t* p){uint32_t v=0;for(int i=3;i>=0;--i)v=v*256+p[i];return v;}
struct Secret {uint8_t bytes[32]{};~Secret(){wipe(bytes,sizeof(bytes));}};
struct Buffers {
 uint8_t* in=nullptr;uint8_t* out=nullptr;size_t bytes;
 // P4 GCM uses DMA directly on these PSRAM buffers. Give each half its own
 // complete cache lines: neither DMA invalidation nor a partial final block
 // may touch the adjacent half or another allocation. The installed P4 SDK
 // uses 128-byte L2 lines (larger than its 64-byte L1 and 16-byte AES blocks).
#if defined(CONFIG_CACHE_L2_CACHE_LINE_SIZE)
 static constexpr size_t Alignment=CONFIG_CACHE_L2_CACHE_LINE_SIZE;
#else
 static constexpr size_t Alignment=128;
#endif
 static_assert(Alignment>=16&&(Alignment&(Alignment-1))==0,"DMA alignment must be a power of two");
 explicit Buffers(size_t n):bytes(((std::max<size_t>(1,std::min(ChunkSize,n))+Alignment-1)&~(Alignment-1))*2){
#ifdef ARDUINO
  in=static_cast<uint8_t*>(heap_caps_aligned_alloc(Alignment,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
  in=static_cast<uint8_t*>(malloc(bytes));
#endif
  if(in)out=in+bytes/2;
 }
 ~Buffers(){if(in){wipe(in,bytes);
#ifdef ARDUINO
  heap_caps_free(in);
#else
  free(in);
#endif
 }}
};
struct Cipher {mbedtls_gcm_context ctx;Cipher(){mbedtls_gcm_init(&ctx);}~Cipher(){mbedtls_gcm_free(&ctx);}};
bool derive(const char* password,const uint8_t* salt,Secret& key,Progress progress=nullptr,void* context=nullptr){
 if(!password||!*password||strlen(password)>256)return false;
 // RFC 8018 PBKDF2, one SHA-256 output block. Keep the exact v1 work factor
 // and byte format, but yield/cancel during derivation rather than afterwards.
 // mbedTLS retains HMAC pads across reset, as its own PKCS5 implementation does.
 mbedtls_md_context_t md;mbedtls_md_init(&md);
 int result=mbedtls_md_setup(&md,mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),1);
 uint8_t block[4]={0,0,0,1};Secret u;
 if(!result)result=mbedtls_md_hmac_starts(&md,reinterpret_cast<const uint8_t*>(password),strlen(password));
 if(!result)result=mbedtls_md_hmac_update(&md,salt,16);
 if(!result)result=mbedtls_md_hmac_update(&md,block,4);
 if(!result)result=mbedtls_md_hmac_finish(&md,u.bytes);
 memcpy(key.bytes,u.bytes,32);
 for(uint32_t i=1;!result&&i<Iterations;++i){
  if((i&1023)==1&&progress&&!progress(0,0,context)){result=-1;break;}
  result=mbedtls_md_hmac_reset(&md);
  if(!result)result=mbedtls_md_hmac_update(&md,u.bytes,32);
  if(!result)result=mbedtls_md_hmac_finish(&md,u.bytes);
  if(!result)for(unsigned j=0;j<32;++j)key.bytes[j]^=u.bytes[j];
 }
 mbedtls_md_free(&md);return result==0;
}
void chunkAuth(const uint8_t* header,size_t hs,uint32_t index,size_t length,uint8_t* nonce,uint8_t* aad){
 memcpy(nonce,header+hs-8,8);for(unsigned i=0;i<4;++i)nonce[8+i]=index>>(24-i*8);
 memcpy(aad,header,hs);put32(aad+hs,index);put32(aad+hs+4,length);
 }
bool fileKey(const uint8_t* master,const uint8_t* salt,Secret& key){
 uint8_t context[24];memcpy(context,"T5VAULT2",8);memcpy(context+8,salt,16);
 return master && mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),master,32,context,sizeof(context),key.bytes)==0;
}
}
static const char* encryptImpl(Stream& in,Stream& out,const char* password,const uint8_t entropy[24],Progress progress,void* context,const uint8_t* master,const uint8_t* id){
 const size_t size=in.size();if(size>128*1024*1024||!entropy)return "Fichier trop volumineux pour le coffre";
 size_t hs=master?KeyHeaderSize:HeaderSize;uint8_t h[KeyHeaderSize]{};
 memcpy(h,master?"T5VAULT2":"T5VAULT1",8);put32(h+8,master?0:Iterations);put32(h+12,ChunkSize);put32(h+16,size);
 if(master){if(!id)return "Clé absente";memcpy(h+24,id,16);memcpy(h+40,entropy,24);}else memcpy(h+24,entropy,24);
 Secret key;if(!(master?fileKey(master,h+40,key):derive(password,h+24,key,progress,context)))return "Échec de dérivation du mot de passe";
 if(progress&&!progress(0,size,context))return "Opération annulée";
 Cipher cipher;if(mbedtls_gcm_setkey(&cipher.ctx,MBEDTLS_CIPHER_ID_AES,key.bytes,256))return "Chiffrement indisponible";
 if(out.write(h,hs)!=hs)return "Écriture du coffre impossible";
 Buffers buffers(size);if(!buffers.in)return "Mémoire insuffisante";size_t position=0;uint32_t index=0;
 do {size_t n=std::min<size_t>(ChunkSize,size-position);uint8_t nonce[12],aad[KeyHeaderSize+8],tag[16];chunkAuth(h,hs,index++,n,nonce,aad);
  if(in.read(buffers.in,n)!=n)return "Lecture du fichier impossible";
  if(mbedtls_gcm_crypt_and_tag(&cipher.ctx,MBEDTLS_GCM_ENCRYPT,n,nonce,12,aad,hs+8,buffers.in,buffers.out,16,tag))return "Chiffrement indisponible";
  if(out.write(buffers.out,n)!=n||out.write(tag,16)!=16)return "Écriture du coffre impossible";
  position+=n;if(progress&&!progress(position,size,context))return "Opération annulée";
 }while(position<size);
 if(in.size()!=size)return "Fichier modifié pendant le chiffrement";
 return nullptr;
}
static const char* decryptImpl(Stream& in,Stream& out,const char* password,Progress progress,void* context,const uint8_t* master,const uint8_t* id){
 size_t hs=master?KeyHeaderSize:HeaderSize;uint8_t h[KeyHeaderSize];
 if(!in.seek(0)||in.read(h,hs)!=hs||memcmp(h,master?"T5VAULT2":"T5VAULT1",8)||get32(h+8)!=(master?0:Iterations)||get32(h+12)!=ChunkSize||get32(h+20))return "Format du coffre invalide";
 if(master&&(!id||memcmp(h+24,id,16)))return "Cette clé ne correspond pas au fichier";
 size_t size=get32(h+16);if(size>128*1024*1024)return "Format du coffre invalide";
 size_t chunks=std::max<size_t>(1,(size+ChunkSize-1)/ChunkSize);
 if(in.size()!=hs+size+chunks*16)return "Coffre tronqué ou altéré";
 Secret key;if(!(master?fileKey(master,h+40,key):derive(password,h+24,key,progress,context)))return "Échec de dérivation du mot de passe";
 Cipher cipher;if(mbedtls_gcm_setkey(&cipher.ctx,MBEDTLS_CIPHER_ID_AES,key.bytes,256))return "Chiffrement indisponible";
 Buffers buffers(size);if(!buffers.in)return "Mémoire insuffisante";
 for(unsigned pass=0;pass<2;++pass){if(!in.seek(hs))return "Lecture du fichier impossible";size_t position=0;
  for(uint32_t index=0;index<chunks;++index){size_t n=std::min<size_t>(ChunkSize,size-position);uint8_t nonce[12],aad[KeyHeaderSize+8],tag[16];chunkAuth(h,hs,index,n,nonce,aad);
   if(in.read(buffers.in,n)!=n||in.read(tag,16)!=16)return "Coffre tronqué ou altéré";
   if(mbedtls_gcm_auth_decrypt(&cipher.ctx,n,nonce,12,aad,hs+8,tag,16,buffers.in,buffers.out))return master?"Clé incorrecte ou coffre altéré":"Mot de passe incorrect ou coffre altéré";
   if(pass&&out.write(buffers.out,n)!=n)return "Écriture du fichier impossible";
   position+=n;if(progress&&!progress(pass*size+position,2*size,context))return "Opération annulée";
  }
 }return nullptr;
}
const char* encrypt(Stream& in,Stream& out,const char* p,const uint8_t e[24],Progress f,void* c){return encryptImpl(in,out,p,e,f,c,nullptr,nullptr);}
const char* decrypt(Stream& in,Stream& out,const char* p,Progress f,void* c){return decryptImpl(in,out,p,f,c,nullptr,nullptr);}
const char* encryptKey(Stream& in,Stream& out,const uint8_t k[32],const uint8_t id[16],const uint8_t e[24],Progress f,void* c){return encryptImpl(in,out,nullptr,e,f,c,k,id);}
const char* decryptKey(Stream& in,Stream& out,const uint8_t k[32],const uint8_t id[16],Progress f,void* c){return decryptImpl(in,out,nullptr,f,c,k,id);}
bool keyId(Stream& in,uint8_t id[16]){uint8_t h[KeyHeaderSize];if(!in.seek(0)||in.read(h,sizeof(h))!=sizeof(h)||memcmp(h,"T5VAULT2",8))return false;memcpy(id,h+24,16);return true;}
bool passwordKey(const char* password,const uint8_t salt[16],uint8_t output[32],Progress progress,void* context){Secret key;if(output)wipe(output,32);if(!salt||!output||!derive(password,salt,key,progress,context))return false;memcpy(output,key.bytes,32);return true;}
}
