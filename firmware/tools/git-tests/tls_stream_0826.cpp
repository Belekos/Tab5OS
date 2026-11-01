#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <string>
#include <cassert>
static unsigned checks=0,closed=0,freed=0,attempt=0,failAt=0;
static bool setupFailure=false;
static std::map<void*,size_t> live;
static void check(bool ok,const char* what){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",what);exit(2);}}
static void* allocate(size_t n){if(++attempt==failAt)return nullptr;void* p=malloc(n);check(p!=nullptr,"host allocation");live[p]=n;return p;}
static void release(void* p){if(!p)return;check(live.erase(p)==1,"double or foreign free");free(p);}
// Preserve production C assignments when compiled by the C++ fault harness.
struct CAllocation{void* pointer;template<class T>operator T*()const{return static_cast<T*>(pointer);}};
static CAllocation git__malloc(size_t n){return {allocate(n)};}
static CAllocation git__calloc(size_t n,size_t size){void* p=allocate(n*size);if(p)memset(p,0,n*size);return {p};}
static void git__free(void* p){release(p);}
static char* git__strdup(const char* s){char* p=(char*)allocate(strlen(s)+1);if(p)strcpy(p,s);return p;}
#define GIT_ERROR_CHECK_ALLOC(p) do{if(!(p))return -1;}while(0)
#define GIT_ASSERT_ARG(p) do{if(!(p))return -1;}while(0)
#define GIT_STREAM_VERSION 1
#define GIT_ERROR_SSL 1
static void git_error_set(int,const char*,...){}
using Callback=int(*)();
struct git_stream{unsigned version;int encrypted,proxy_support;Callback connect,certificate,set_proxy,read,write,close;void(*free)(git_stream*);};
struct mbedtls_ssl_context{void* buffer;};
static int mbedtls_config;
struct mbedtls_stream{git_stream parent;git_stream* io;int owned;mbedtls_ssl_context* ssl;char* host;struct{void* data;}cert_info;bool connected;};
static void mbedtls_ssl_init(mbedtls_ssl_context* p){p->buffer=nullptr;}
static int mbedtls_ssl_setup(mbedtls_ssl_context* p,const int*){p->buffer=allocate(4096);return !p->buffer||setupFailure?-1:0;}
static void mbedtls_ssl_free(mbedtls_ssl_context* p){release(p->buffer);p->buffer=nullptr;}
static int git_stream_supports_proxy(git_stream*){return 0;}
static int git_stream_close(git_stream* p){check(live.count(p)==1,"socket close after free");++closed;return 0;}
static void git_stream_free(git_stream* p){++freed;release(p);}
static int git_socket_stream_new(git_stream** out,const char*,const char*){*out=(git_stream*)allocate(sizeof(git_stream));return *out?0:-1;}
static int mbedtls_connect(){return 0;}static int mbedtls_certificate(){return 0;}static int mbedtls_set_proxy(){return 0;}
static int mbedtls_stream_read(){return 0;}static int mbedtls_stream_write(){return 0;}static int mbedtls_stream_close(){return 0;}
/* PRODUCTION */
int main(int argc,char**){
 // Baseline deliberately selects the exact setup-failure path observed in
 // the source audit, so a regression proof does not stop at a separate leak.
 const bool baseline=argc>1;
 for(unsigned cycle=0;cycle<(baseline?1:100);++cycle){
  for(bool borrowed:{false,true})for(unsigned fault=0;fault<=6;++fault){
   if(baseline&&(borrowed||fault!=6))continue;
   check(live.empty(),"no allocation retained by previous attempt");closed=freed=attempt=0;
   setupFailure=fault==6;failAt=fault==6?0:fault;
   git_stream* socket=nullptr,*result=nullptr;int rc;
   if(borrowed){failAt=0;check(git_socket_stream_new(&socket,"example.invalid","443")==0,"borrowed socket");attempt=0;failAt=fault==6?0:fault;rc=git_mbedtls_stream_wrap(&result,socket,"example.invalid");}
   else rc=git_mbedtls_stream_new(&result,"example.invalid","443");
   if(rc==0){check(result!=nullptr,"success publishes stream");mbedtls_stream_free(result);}
   else check(result==nullptr,"failure publishes no partial stream");
   if(borrowed){check(live.count(socket)==1,"borrowed socket remains owned by caller");check(closed==0&&freed==0,"wrapper never closes borrowed socket");git_stream_free(socket);}
   check(live.empty(),"all allocation failure paths release SSL buffers and wrapper");
   check(freed<=1,"each socket released at most once");
   // Recovery in the same process exercises global/context lifetime too.
   failAt=0;setupFailure=false;attempt=closed=freed=0;result=nullptr;
   check(git_mbedtls_stream_new(&result,"example.invalid","443")==0,"healthy attempt follows failure");mbedtls_stream_free(result);
   check(live.empty()&&freed==1,"healthy attempt released exactly once");
  }
 }
 printf("PASS %u checks: 1400 TLS constructor paths, 1400 healthy retries; owned/borrowed sockets, no retained allocation.\n",checks);
}
