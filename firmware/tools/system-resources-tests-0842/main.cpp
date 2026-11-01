#include "Tab5SystemResources.h"
#include "esp_heap_caps.h"
#include "Tab5ResourceManager.h"
#include "src/tab5git/deps/zlib/zlib.h"
#include "Tab5SystemResources.cpp"
#include "Tab5Storage.h"
#include "Tab5VolumeFS.h"
#include <cassert>
#include <cstdio>
#include <thread>

fs::FS tab5FS;
void finish(){unsigned limit=10000;while(Tab5SystemResources::busy()&&--limit)Tab5SystemResources::service();assert(limit);assert(memory::blocks.empty());assert(admission::active==0);}
void clear(){Tab5SystemResources::cancel();mock::files.clear();mock::dirs={"/"};mock::shortWrite=mock::corruptRead=mock::renameFailure=mock::flushError=mock::closeError=false;mock::writes=0;assert(memory::blocks.empty());assert(admission::active==0);memory::failAt=memory::calls=0;admission::deny=false;}
bool hasTemps(){for(const auto& file:mock::files)if(file.first.find(".tab5-repair-")!=std::string::npos)return true;return false;}
std::string expected(const Tab5SystemResource& e){std::string s(e.size,'\0');z_stream z{};assert(inflateInit(&z)==Z_OK);z.next_in=const_cast<Bytef*>(e.data);z.avail_in=e.packedSize;z.next_out=reinterpret_cast<Bytef*>(s.data());z.avail_out=e.size;assert(inflate(&z,Z_FINISH)==Z_STREAM_END);assert(z.total_out==e.size);assert(inflateEnd(&z)==Z_OK);return s;}
int main(){
 assert(memory::calls==0);Tab5SystemResources::service();assert(memory::calls==0);
 size_t largest=0;for(size_t i=1;i<Tab5SystemResources::total();++i)if(TAB5_SYSTEM_RESOURCES[i].size>TAB5_SYSTEM_RESOURCES[largest].size)largest=i;
 for(unsigned fault=1;fault<=4;++fault){clear();std::swap(TAB5_SYSTEM_RESOURCES[0],TAB5_SYSTEM_RESOURCES[largest]);memory::failAt=fault;assert(Tab5SystemResources::start(true));finish();assert(Tab5SystemResources::created()==0);assert(mock::files.empty());assert(!hasTemps());std::swap(TAB5_SYSTEM_RESOURCES[0],TAB5_SYSTEM_RESOURCES[largest]);clear();assert(Tab5SystemResources::start(true));finish();assert(Tab5SystemResources::created()==40);}
 clear();admission::deny=true;assert(Tab5SystemResources::start(true));finish();assert(mock::files.empty());
 for(int fault=0;fault<6;++fault){clear();auto& e=TAB5_SYSTEM_RESOURCES[0];auto saved=e;auto* data=const_cast<uint8_t*>(e.data);const auto last=data[e.packedSize-1];
 if(fault==0)--e.packedSize; if(fault==1)--e.size; if(fault==2)++e.size; if(fault==3)data[e.packedSize-1]^=1; if(fault==4)e.size=128*1024+1;if(fault==5)e.packedSize=0;
 assert(Tab5SystemResources::start(true));finish();assert(Tab5SystemResources::created()==0);assert(mock::files.empty());assert(!hasTemps());data[saved.packedSize-1]=last;e=saved;}

 assert(!Tab5SystemResources::start(false));assert(!Tab5SystemResources::busy());
 clear();assert(Tab5SystemResources::start(true));finish();
 assert(Tab5SystemResources::created()==Tab5SystemResources::total());
 for(const auto& entry:TAB5_SYSTEM_RESOURCES){assert(mock::files[entry.path]==expected(entry));}
 assert(!hasTemps());
 mock::files[TAB5_SYSTEM_RESOURCES[0].path]="CUSTOM PERSONAL CONTENT";
 mock::files[TAB5_SYSTEM_RESOURCES[1].path]="";
 const auto before=mock::files;const auto writes=mock::writes;
 assert(Tab5SystemResources::start(true));finish();assert(Tab5SystemResources::created()==0);
 assert(mock::files==before);assert(mock::writes==writes);
 for(unsigned failure=0;failure<2;++failure){clear();mock::flushError=failure==0;mock::closeError=failure==1;assert(Tab5SystemResources::start(true));finish();assert(Tab5SystemResources::created()==0);assert(mock::files.empty());assert(!hasTemps());}
 clear();mock::shortWrite=true;assert(Tab5SystemResources::start(true));finish();
 assert(Tab5SystemResources::created()==0);assert(mock::files.empty());assert(!hasTemps());
 clear();mock::corruptRead=true;assert(Tab5SystemResources::start(true));finish();
 assert(Tab5SystemResources::created()==0);assert(mock::files.empty());
 clear();mock::renameFailure=true;assert(Tab5SystemResources::start(true));finish();
 assert(Tab5SystemResources::created()==0);assert(mock::files.empty());
 clear();assert(Tab5SystemResources::start(true));Tab5SystemResources::service();Tab5SystemResources::service();
 Tab5SystemResources::cancel();assert(!Tab5SystemResources::busy());assert(!hasTemps());
 clear();assert(Tab5SystemResources::start(true));Tab5SystemResources::service();
 mock::files[TAB5_SYSTEM_RESOURCES[0].path]="RACING USER FILE";finish();
 assert(mock::files[TAB5_SYSTEM_RESOURCES[0].path]=="RACING USER FILE");assert(!hasTemps());
 clear();mock::files["/MicroPython"]="PERSONAL FILE AT DIRECTORY PATH";
 assert(Tab5SystemResources::start(true));finish();assert(mock::files["/MicroPython"]=="PERSONAL FILE AT DIRECTORY PATH");
 {Tab5StorageUse use;Tab5StorageRecovery blocked;assert(use);assert(!blocked);}
 {Tab5StorageRecovery owner;assert(owner);assert(tab5StorageExclusiveOwner());Tab5StorageUse blocked;assert(!blocked);
  bool otherOwner=true;std::thread worker([&]{otherOwner=tab5StorageExclusiveOwner();});worker.join();assert(!otherOwner);}
 assert(!tab5StorageExclusiveOwner());
 tab5StorageBootResult(true,0);tab5StorageEjected();tab5StorageFault("test","/",ENODEV);assert(!tab5StorageRecoveryDue(100000));
 tab5StorageRequestRecovery();assert(tab5StorageRecoveryDue(100000));tab5StorageRecoveryDone(100000,true);
 assert(!tab5StorageRecoveryDue(200000));
 printf("peak PSRAM bytes: %zu\n",memory::peak);
 std::puts("PASS 26 cases: flush/close errno errors without write-error flag, admission denial, four allocation failures and retry, six stream corruption/bounds failures, idle has no allocation;  absent SD, empty SD, existing/custom/empty files, short write, corrupt readback, failed commit, cancellation, racing destination, directory collision, exclusive owner, deliberate eject, manual remount");
}
