#include <cassert>
#include <cstdio>
#include <cstdlib>
#define GIT_SYSTEM_CALL
using git_tlsdata_key = int;
#include "thread-production.inc"
static unsigned checks=0, destroyed=0;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"check line %d: %s\n",__LINE__,#x);std::abort();} } while(0)
static int payload=42;
static void destroy(void* p){CHECK(p==&payload);++destroyed;}
static int nestedKey=-1;
static void reentrant(void* p){CHECK(p==&payload);++destroyed;CHECK(git_tlsdata_init(&nestedKey,destroy)==0);CHECK(git_tlsdata_set(nestedKey,&payload)==0);}
int main(){
 CHECK(git_tlsdata_init(nullptr,destroy)==-1);
 for(int cycle=0;cycle<1000;++cycle){
  int keys[16];
  for(int i=0;i<16;++i){CHECK(git_tlsdata_init(keys+i,destroy)==0);CHECK(keys[i]==i);CHECK(git_tlsdata_get(keys[i])==nullptr);CHECK(git_tlsdata_set(keys[i],&payload)==0);}
  int extra=-9;CHECK(git_tlsdata_init(&extra,destroy)==-1);CHECK(extra==-9);
  for(int bad:{-1,16,17,2147483647}){CHECK(git_tlsdata_set(bad,&payload)==-1);CHECK(git_tlsdata_get(bad)==nullptr);CHECK(git_tlsdata_dispose(bad)==-1);}
  CHECK(git_tlsdata_dispose(keys[7])==0);CHECK(git_tlsdata_dispose(keys[7])==-1);CHECK(git_tlsdata_get(keys[7])==nullptr);CHECK(git_tlsdata_set(keys[7],&payload)==-1);
  CHECK(git_tlsdata_init(&extra,destroy)==0);CHECK(extra==7);CHECK(git_tlsdata_get(extra)==nullptr);CHECK(git_tlsdata_set(extra,&payload)==0);
  for(int i=15;i>=0;--i)CHECK(git_tlsdata_dispose(keys[i])==0);
 }
 CHECK(destroyed==17000);
 int key=-1;CHECK(git_tlsdata_init(&key,reentrant)==0);CHECK(git_tlsdata_set(key,&payload)==0);CHECK(git_tlsdata_dispose(key)==0);CHECK(nestedKey==key);CHECK(git_tlsdata_get(nestedKey)==&payload);CHECK(git_tlsdata_dispose(nestedKey)==0);
 CHECK(destroyed==17002);
 CHECK(git_tlsdata_init(&key,destroy)==0);CHECK(git_tlsdata_set(key,nullptr)==0);CHECK(git_tlsdata_dispose(key)==0);CHECK(destroyed==17002);
 std::printf("{\"passed\":true,\"checks\":%u,\"cycles\":1000,\"destructorCalls\":%u,\"simultaneousKeys\":16}\n",checks,destroyed);
}
