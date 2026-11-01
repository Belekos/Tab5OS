#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <malloc.h>
namespace heap {
struct Allocation{void* base;size_t bytes;};
std::map<void*,Allocation> live;
unsigned wiped=0,violations=0;bool fail=false;
void* allocate(size_t alignment,size_t bytes,bool unaligned){
 if(fail)return nullptr;
 void* base=_aligned_malloc(bytes+128,std::max<size_t>(alignment,128));
 if(!base)return nullptr;
 void* result=static_cast<unsigned char*>(base)+(unaligned?4:0);
 live[result]={base,bytes};return result;
}
void release(void* p){if(!p)return;auto at=live.find(p);if(at==live.end())abort();
 for(size_t i=0;i<at->second.bytes;++i)if(static_cast<unsigned char*>(p)[i]){++violations;break;}
 ++wiped;_aligned_free(at->second.base);live.erase(at);
}
}
#include "Tab5VaultCore.cpp"
static unsigned checks=0,alignmentFailures=0;
static void check(bool ok){++checks;if(!ok){fprintf(stderr,"check %u failed\n",checks);exit(2);}}
struct Memory:tab5vault::Stream{
 std::vector<uint8_t> data;size_t at=0;
 size_t read(void* p,size_t n)override{n=std::min(n,data.size()-at);memcpy(p,data.data()+at,n);at+=n;return n;}
 size_t write(const void* p,size_t n)override{if(at+n>data.size())data.resize(at+n);memcpy(data.data()+at,p,n);at+=n;return n;}
 bool seek(size_t n)override{if(n>data.size())return false;at=n;return true;}
 size_t size()override{return data.size();}
};
int main(){
 for(size_t size=0;size<=8193;++size){
  tab5vault::Buffers b(size);check(b.in&&b.out&&b.bytes<=8192);
  if(uintptr_t(b.in)%128||uintptr_t(b.out)%128||b.bytes%256)++alignmentFailures;
  const size_t n=std::max<size_t>(1,std::min<size_t>(4096,size));check(size_t(b.out-b.in)>=n);
  memset(b.in,0x65,b.bytes);
 }
 check(heap::live.empty()&&heap::violations==0);
 heap::fail=true;{tab5vault::Buffers b(4096);check(!b.in&&!b.out);}heap::fail=false;
 const size_t sizes[]={0,1,15,16,17,127,128,129,799,800,801,4095,4096,4097,8193};
 uint8_t key[32]{},id[16]{},entropy[24]{};for(unsigned i=0;i<32;++i)key[i]=uint8_t(i*7+3);
 for(size_t size:sizes){Memory plain,cipher,restored;plain.data.resize(size);for(size_t i=0;i<size;++i)plain.data[i]=uint8_t(i*71+5);
  check(tab5vault::encryptKey(plain,cipher,key,id,entropy)==nullptr);
  check(tab5vault::decryptKey(cipher,restored,key,id)==nullptr&&restored.data==plain.data);
  if(size){cipher.data.back()^=1;Memory rejected;check(tab5vault::decryptKey(cipher,rejected,key,id)!=nullptr&&rejected.data.empty());}
 }
 check(heap::live.empty()&&heap::violations==0);
 printf("{\"checks\":%u,\"alignmentFailures\":%u,\"wipedAllocations\":%u,\"wipeViolations\":%u}\n",checks,alignmentFailures,heap::wiped,heap::violations);
 return alignmentFailures?1:0;
}
