#include "Tab5SystemResources.h"
#include "Tab5SystemResourcesData.h"
#include "Tab5Storage.h"
#include "Tab5VolumeFS.h"
#include <cassert>
#include <cstdio>
#include <thread>

fs::FS tab5FS;
void finish(){unsigned limit=10000;while(Tab5SystemResources::busy()&&--limit)Tab5SystemResources::service();assert(limit);}
void clear(){Tab5SystemResources::cancel();mock::files.clear();mock::dirs={"/"};mock::shortWrite=mock::corruptRead=mock::renameFailure=false;mock::writes=0;}
bool hasTemps(){for(const auto& file:mock::files)if(file.first.find(".tab5-repair-")!=std::string::npos)return true;return false;}
int main(){
 assert(!Tab5SystemResources::start(false));assert(!Tab5SystemResources::busy());
 clear();assert(Tab5SystemResources::start(true));finish();
 assert(Tab5SystemResources::created()==Tab5SystemResources::total());
 for(const auto& entry:TAB5_SYSTEM_RESOURCES){assert(mock::files[entry.path]==std::string(entry.data,entry.size));}
 assert(!hasTemps());
 mock::files[TAB5_SYSTEM_RESOURCES[0].path]="CUSTOM PERSONAL CONTENT";
 mock::files[TAB5_SYSTEM_RESOURCES[1].path]="";
 const auto before=mock::files;const auto writes=mock::writes;
 assert(Tab5SystemResources::start(true));finish();assert(Tab5SystemResources::created()==0);
 assert(mock::files==before);assert(mock::writes==writes);
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
 std::puts("PASS 12 cases: absent SD, empty SD, existing/custom/empty files, short write, corrupt readback, failed commit, cancellation, racing destination, directory collision, exclusive owner, deliberate eject, manual remount");
}
