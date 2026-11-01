#include "TestCache.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
constexpr uintptr_t ExternalBegin = 0x49000000, ExternalEnd = 0x4b000000;
constexpr uintptr_t CacheBegin = 0x48000000, CacheEnd = 0x4c000000;
constexpr size_t Chunk = 16384;
struct Call { uintptr_t address; size_t size; int flags; };
struct Operation { uint32_t address, size; char kind; };
std::vector<Call> calls;
std::vector<Operation> operations;
bool isr = false, canYield = true, rejectWhole = false;
uint32_t line = 128;
unsigned depth = 0, exits = 0, failureCall = 0, checks = 0, cases = 0;
size_t maxLockedBytes = 0;
void require(bool ok) { ++checks; if (!ok) { std::fprintf(stderr,"assertion %u failed (case %u)\n",checks,cases); std::abort(); } }
void reset() { calls.clear(); operations.clear(); depth=exits=0; maxLockedBytes=0; failureCall=0; }
struct Interval { uint64_t first,end; bool operator==(const Interval& o) const{return first==o.first&&end==o.end;} };
std::vector<Interval> covered(const std::vector<Operation>& ops,char kind) {
  std::vector<Interval> result;
  for(const auto& op:ops) if(op.kind==kind && op.size) {
    Interval span{op.address/line, (uint64_t(op.address)+op.size+line-1)/line};
    if(!result.empty()&&result.back().end==span.first)result.back().end=span.end;
    else result.push_back(span);
  }
  return result;
}
void compare(uintptr_t addr,size_t size,int flags,bool split) {
  ++cases;reset();
  const int expected=tab5_native_cache_msync(reinterpret_cast<void*>(addr),size,flags);
  const auto expectedOperations=operations;
  reset();
  const int actual=__wrap_esp_cache_msync(reinterpret_cast<void*>(addr),size,flags);
  require(actual==expected);require(depth==0);
  if(split) {
    require(expected==ESP_OK);require(calls.size()>1);require(maxLockedBytes<=Chunk);
    for(const auto& call:calls)require(call.size>0&&call.size<=Chunk);
    require(exits==operations.size());
  } else {
    require(calls.size()==1);require(calls[0].address==addr);
    require(calls[0].size==size);require(calls[0].flags==flags);
  }
  require(covered(operations,'W')==covered(expectedOperations,'W'));
  require(covered(operations,'I')==covered(expectedOperations,'I'));
  bool invalidated=false;
  for(const auto& op:operations) {if(op.kind=='I')invalidated=true;else require(!invalidated);}
  if(expected!=ESP_OK)require(operations.empty());
}
}
extern "C" bool xPortInIsrContext(){return isr;}
extern "C" bool xPortCanYield(){return canYield;}
extern "C" bool esp_ptr_external_ram(const void* ptr){const auto p=reinterpret_cast<uintptr_t>(ptr);return p>=ExternalBegin&&p<ExternalEnd;}
extern "C" bool cache_hal_vaddr_to_cache_level_id(uint32_t addr,uint32_t count,uint32_t* level,uint32_t* id){
  if(rejectWhole&&count>Chunk)return false;
  // This instrumented HAL boundary has contiguous native-cache and PSRAM
  // ranges. The tested validator and ordering are the actual SDK function.
  const uint64_t end=uint64_t(addr)+count-1;
  if(addr<CacheBegin||end>=CacheEnd)return false;
  *level=2;*id=3;return true;
}
extern "C" uint32_t cache_hal_get_cache_line_size(uint32_t,cache_type_t){return line;}
extern "C" void test_enter_critical(int*){require(depth==0);++depth;}
extern "C" void test_exit_critical(int*){require(depth==1);--depth;++exits;}
bool record(uint32_t addr,uint32_t count,char kind){
  require(depth==1);uint32_t level,id;
  const bool valid=cache_hal_vaddr_to_cache_level_id(addr,count,&level,&id);
  if(valid){operations.push_back({addr,count,kind});
    const auto coveredBytes=((uint64_t(addr)+count+line-1)/line-addr/line)*line;
    maxLockedBytes=std::max(maxLockedBytes,size_t(coveredBytes));}
  return valid;
}
extern "C" bool cache_hal_writeback_addr(uint32_t addr,uint32_t count){return record(addr,count,'W');}
extern "C" bool cache_hal_invalidate_addr(uint32_t addr,uint32_t count){return record(addr,count,'I');}
extern "C" esp_err_t __real_esp_cache_msync(void* addr,size_t size,int flags){
  require(depth==0);calls.push_back({reinterpret_cast<uintptr_t>(addr),size,flags});
  if(failureCall&&calls.size()==failureCall)return 0x777;
  return tab5_native_cache_msync(addr,size,flags);
}
int main(){
  const int C=ESP_CACHE_MSYNC_FLAG_DIR_C2M,M=ESP_CACHE_MSYNC_FLAG_DIR_M2C;
  const int U=ESP_CACHE_MSYNC_FLAG_UNALIGNED,I=ESP_CACHE_MSYNC_FLAG_INVALIDATE;
  const int D=ESP_CACHE_MSYNC_FLAG_TYPE_DATA,T=ESP_CACHE_MSYNC_FLAG_TYPE_INST;
  for(uint32_t cacheLine:{64u,128u,256u}) {
    line=cacheLine;
    for(size_t size:{size_t(0),size_t(128),Chunk,Chunk+256,size_t(1843200),size_t(32*1024*1024)}) {
      if(size%line)continue;
      for(int flags:{0,C,D,C|D,M,M|D,I,C|I,C|I|D,C|I|U,M|I})
        compare(ExternalBegin,size,flags,size>Chunk);
    }
    for(uintptr_t offset:{uintptr_t(1),uintptr_t(3),uintptr_t(cacheLine-1)}) {
      for(size_t tail:{size_t(0),size_t(1),size_t(127)}) {
        compare(ExternalBegin+offset,Chunk*3+tail,C|U,true);
        compare(ExternalBegin+offset,Chunk*3+tail,U,true);
        compare(ExternalBegin+offset,Chunk*3+tail,C|U|I,false);
        compare(ExternalBegin+offset,Chunk*3+tail,C,false);
      }
    }
  }
  line=128;
  for(unsigned flags=0;flags<256;++flags){
    const bool split=!(flags&~unsigned(63))&&!(flags&T)&&!((flags&M)&&(flags&C))&&!((flags&M)&&(flags&U));
    compare(ExternalBegin,Chunk*3,flags,split);
  }
  for(size_t size:{size_t(1),Chunk,Chunk+1,Chunk*3}) {
    compare(0,size,C,false);compare(UINT32_MAX-256,size,C,false);
    compare(CacheBegin,size,C|U,false);
    compare(ExternalEnd-128,size,C|U,false);
    compare(CacheEnd-128,size,C|U,false);
  }
  compare(ExternalBegin,SIZE_MAX,C,false);
  rejectWhole=true;compare(ExternalBegin,Chunk*3,C,false);rejectWhole=false;
  isr=true;compare(ExternalBegin,Chunk*3,C,false);isr=false;
  canYield=false;compare(ExternalBegin,Chunk*3,M,false);canYield=true;
  line=3;compare(ExternalBegin,Chunk*3,C|U,false);line=128;
  // A failed native chunk must abort before later chunks or invalidation.
  for(int flags:{C,M,C|I})for(unsigned fail:{1u,2u,3u,4u,5u,6u}) {
    if(flags!= (C|I)&&fail>3)continue;
    ++cases;reset();failureCall=fail;
    require(__wrap_esp_cache_msync(reinterpret_cast<void*>(ExternalBegin),Chunk*3,flags)==0x777);
    require(calls.size()==fail);require(depth==0);
    if(flags==(C|I)&&fail<=3)require(covered(operations,'I').empty());
  }
  std::printf("cache sync: %u differential/fault cases, %u assertions; native SDK validator and HAL operation order executed\n",cases,checks);
}
