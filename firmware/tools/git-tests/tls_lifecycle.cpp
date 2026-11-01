#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <map>
#include <string>
#include <iostream>
#include <sys/stat.h>
#define ESP_PLATFORM 1
#define GIT_DEFAULT_CERT_LOCATION "tls-ca.pem"
#define GIT_ERROR_SSL 1
#define MBEDTLS_SSL_IS_CLIENT 1
#define MBEDTLS_SSL_TRANSPORT_STREAM 1
#define MBEDTLS_SSL_PRESET_DEFAULT 1
#define MBEDTLS_SSL_VERIFY_REQUIRED 2
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#ifndef S_ISREG
#define S_ISREG(m) (((m)&_S_IFMT)==_S_IFREG)
#define S_ISDIR(m) (((m)&_S_IFMT)==_S_IFDIR)
#endif
struct Context{void* first=nullptr;void* second=nullptr;};
using mbedtls_ssl_config=Context;
using mbedtls_entropy_context=Context;
using mbedtls_ctr_drbg_context=Context;
using mbedtls_x509_crt=Context;
enum Fault{None,Defaults,Bundle,Dup,Ciphers,Seed,Register,AllCiphers};
static Fault failure=None;
static std::map<void*,size_t> live;
static size_t checks=0,bytes=0,peak=0;
static void (*registered)()=nullptr;
static void check(bool value,const char* what){++checks;if(!value){std::cerr<<what<<"\n";std::exit(2);}}
static void* tracked(size_t count){void* p=std::malloc(count);check(p!=nullptr,"harness allocation");live[p]=count;bytes+=count;if(bytes>peak)peak=bytes;return p;}
static void release(void* p){if(!p)return;auto it=live.find(p);check(it!=live.end(),"double or foreign free");bytes-=it->second;live.erase(it);std::free(p);}
static void freeContext(Context* c){release(c->first);release(c->second);*c={};}
static void mbedtls_ssl_config_init(Context* c){*c={};}
static void mbedtls_entropy_init(Context* c){*c={};c->first=tracked(64);}
static void mbedtls_ctr_drbg_init(Context* c){*c={};c->first=tracked(48);}
static int mbedtls_ssl_config_defaults(Context* c,int,int,int){c->first=tracked(128);return failure==Defaults?-1:0;}
static void mbedtls_ssl_config_free(Context* c){freeContext(c);}
static void mbedtls_entropy_free(Context* c){freeContext(c);}
static void mbedtls_ctr_drbg_free(Context* c){freeContext(c);}
static void mbedtls_x509_crt_free(Context* c){freeContext(c);}
static void mbedtls_ssl_conf_authmode(Context*,int mode){check(mode==MBEDTLS_SSL_VERIFY_REQUIRED,"certificate verification required");}
static int esp_crt_bundle_attach(Context*){return failure==Bundle?-1:0;}
static char* git__strdup(const char* s){if(failure==Dup)return nullptr;size_t n=strlen(s)+1;auto p=static_cast<char*>(tracked(n));memcpy(p,s,n);return p;}
static void git__free(void* p){release(p);}
static char* git__strtok(char** rest,const char* separators){if(!*rest)return nullptr;char* start=*rest;char* delimiter=strpbrk(start,separators);if(delimiter){*delimiter=0;*rest=delimiter+1;}else *rest=nullptr;return start;}
static int mbedtls_ssl_get_ciphersuite_id(const char* s){if(failure==Ciphers)return 0;if(failure==AllCiphers)return 10;return strstr(s,"TLS1-3-")?0:10;}
static void git_error_set(int,const char*,...){}
static int mbedtls_entropy_func(void*,unsigned char*,size_t){return 0;}
static int mbedtls_ctr_drbg_random(void*,unsigned char*,size_t){return 0;}
static int mbedtls_ctr_drbg_seed(Context* c,int(*)(void*,unsigned char*,size_t),void*,const unsigned char*,size_t){c->second=tracked(96);return failure==Seed?-1:0;}
static void mbedtls_ssl_conf_rng(Context*,int(*)(void*,unsigned char*,size_t),void*){}
static void mbedtls_ssl_conf_ciphersuites(Context*,const int*);
static int git_mbedtls__set_cert_location(const char*,const char*);
static int git_runtime_shutdown_register(void(*callback)()){if(failure==Register)return -1;registered=callback;return 0;}
#include "tls-production.inc"
static int git_mbedtls__set_cert_location(const char*,const char*){has_ca_chain=true;mbedtls_ca_chain.first=tracked(80);return 0;}
static void mbedtls_ssl_conf_ciphersuites(Context*,const int* list){bool terminated=false;for(size_t n=0;n<ARRAY_SIZE(ciphers_list);++n)if(!list[n]){terminated=true;break;}check(terminated,"cipher list must contain a zero terminator inside its bounds");}
int main(){
 for(unsigned cycle=0;cycle<200;++cycle){for(Fault f:{None,Defaults,Bundle,Dup,Ciphers,Seed,Register,AllCiphers}){
  failure=f;registered=nullptr;check(bytes==0&&live.empty(),"clean entry");
  int rc=git_mbedtls_stream_global_init();bool success=f==None||f==AllCiphers;
  check(success?rc==0:rc<0,"injected failure propagated");
  if(success){check(initialized&&registered&&has_ca_chain,"successful ownership published");registered();}
  else check(!initialized&&!registered,"failure leaves no shutdown registration");
  shutdown_ssl();shutdown_ssl();check(!initialized&&!has_ca_chain,"shutdown idempotent");check(bytes==0&&live.empty(),"TLS/libgit2 allocation retained");
 }}
 std::cout<<"{\"passed\":true,\"assertions\":"<<checks<<",\"cycles\":1600,\"retainedBytes\":"<<bytes<<",\"peakTrackedBytes\":"<<peak<<"}\n";
}
