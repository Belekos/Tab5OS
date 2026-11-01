#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5UsbStorage.h"
#include <cassert>
#include <thread>
#include <atomic>
#include <cstdio>
static unsigned checks=0;
#define REQUIRE(x) do{++checks;assert(x);}while(0)
int main(){
 const auto revision=tab5FsRevision();
 REQUIRE(tab5SdOpenFiles()==0);
 // Successful read handles keep their volume alive through shared copies.
 for(unsigned n=0;n<1000;++n){
  auto directory=tab5FS.open("/");auto next=directory->openNextFile("r");
  REQUIRE(directory&&next&&tab5SdOpenFiles()==2);
  auto alias=next;next.reset();REQUIRE(tab5SdOpenFiles()==2);
  directory.reset();REQUIRE(tab5SdOpenFiles()==1);
  alias.reset();REQUIRE(tab5SdOpenFiles()==0);
 }
 REQUIRE(tab5FsRevision()==revision);
 // Both path opens and directory enumeration can be denied by a live recovery
 // owner. A rejected acquisition must identify contention, not stale errno.
 int openError=0,existsError=0,nextError=0;
 {
  auto directory=tab5FS.open("/");REQUIRE(directory);
  Tab5StorageRecovery recovery;REQUIRE(recovery&&tab5StorageExclusiveOwner());
  std::thread worker([&]{
   errno=0;auto denied=tab5FS.open("/one");openError=errno;REQUIRE(!denied);
   errno=0;bool exists=tab5FS.exists("/one");existsError=errno;REQUIRE(!exists);
   errno=0;auto deniedNext=directory->openNextFile("r");nextError=errno;REQUIRE(!deniedNext);
  });worker.join();
  // Owner remains able to run its probe; a rejected worker acquires no refs.
  auto probe=tab5FS.open("/probe");REQUIRE(probe&&tab5SdOpenFiles()==2);
 }
 REQUIRE(tab5SdOpenFiles()==0);
 // A shared FileImpl can outlive explicit close. Reads must report EBADF.
 int closedError=0;
 {auto closed=tab5FS.open("/one");REQUIRE(closed);auto alias=closed;
  closed->close();uint8_t byte;errno=0;REQUIRE(alias->read(&byte,1)==0);closedError=errno;
  REQUIRE(tab5SdOpenFiles()==0);}
 tab5StorageBootResult(true,0);tab5StorageFault("closed file","/one",closedError);
 const bool recoveryAfterClosed=tab5StorageRecoveryDue(3000);
 tab5StorageBootResult(true,0);tab5StorageFault("bad descriptor","/one",EBADF);
 const bool recoveryAfterBadFd=tab5StorageRecoveryDue(3000);
 // Resource/path failures never request a bus remount; communication failures do.
 for(int error:{EBUSY,ENOMEM,EMFILE,ENFILE,ENOSPC,EROFS,EACCES,EEXIST,ENOENT,EINVAL}){
  REQUIRE(!tab5StorageTransportError(error));
  tab5StorageBootResult(true,0);tab5StorageFault("operation","/one",error);
  REQUIRE(!tab5StorageRecoveryDue(3000));REQUIRE(tab5StorageLastError()==error);
 }
 REQUIRE(!tab5StorageTransportError(0));REQUIRE(!tab5StorageTransportError(EAGAIN));REQUIRE(!tab5StorageTransportError(EBADF));
 for(int error:{EIO,ENODEV,ENXIO,ETIMEDOUT}){
  REQUIRE(tab5StorageTransportError(error));
  tab5StorageBootResult(true,0);tab5StorageFault("operation","/one",error);
  REQUIRE(!tab5StorageRecoveryDue(2999));REQUIRE(tab5StorageRecoveryDue(3000));
  // Bounded recovery behavior remains intact and successful completion clears it.
  tab5StorageRecoveryDone(3000,false);REQUIRE(!tab5StorageRecoveryDue(32999));
  REQUIRE(tab5StorageRecoveryDue(33000));tab5StorageRecoveryDone(33000,true);
  REQUIRE(!tab5StorageRecoveryDue(100000));
 }
 tab5StorageEjected();tab5StorageFault("ejected","/one",EIO);REQUIRE(!tab5StorageRecoveryDue(100000));
 // Generation guards reject stale handles without confusing them with closed ones.
 {auto file=tab5FS.open("/one");tab5SdSetAvailable(false);tab5SdSetAvailable(true);
  errno=0;uint8_t byte;REQUIRE(!file->read(&byte,1)&&errno==ENODEV);}
 REQUIRE(tab5SdOpenFiles()==0);
 // Ordinary leases still prevent the exclusive owner from starting.
 {Tab5StorageUse use;REQUIRE(use);Tab5StorageRecovery denied;REQUIRE(!denied);}
 {Tab5StorageRecovery free;REQUIRE(free);}
#ifdef EXPECT_FIXED
 REQUIRE(openError==EBUSY&&existsError==EBUSY&&nextError==EBUSY);
 REQUIRE(closedError==EBADF&&!recoveryAfterClosed&&!recoveryAfterBadFd);
#endif
 std::printf("{\"checks\":%u,\"cycles\":1000,\"openErrno\":%d,\"existsErrno\":%d,\"enumerationErrno\":%d,\"closedErrno\":%d,\"remountRequestedAfterClosed\":%s,\"remountRequestedAfterEBADF\":%s,\"retainedSdRefs\":%u}\n",checks,openError,existsError,nextError,closedError,recoveryAfterClosed?"true":"false",recoveryAfterBadFd?"true":"false",tab5SdOpenFiles());
}
