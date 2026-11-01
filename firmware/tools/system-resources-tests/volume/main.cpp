#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5UsbStorage.h"
#include <cassert>
#include <thread>
#include <cstdio>
int main(){
 const auto initialRevision=tab5FsRevision();
 assert(tab5SdOpenFiles()==0);
 assert(tab5FS.exists("/one"));assert(tab5SdOpenFiles()==0);
 {auto missing=tab5FS.open("/missing");assert(!missing);assert(tab5SdOpenFiles()==0);}
 {auto one=tab5FS.open("/one");assert(one);assert(tab5SdOpenFiles()==1);
  auto copy=one;one.reset();assert(tab5SdOpenFiles()==1);copy.reset();assert(tab5SdOpenFiles()==0);}
 {auto one=tab5FS.open("/one");auto copy=one;copy->close();assert(tab5SdOpenFiles()==0);assert(!bool(*one));}
 {auto directory=tab5FS.open("/");auto child=directory->openNextFile("r");assert(child);assert(tab5SdOpenFiles()==2);
  assert(!directory->openNextFile("r"));assert(tab5SdOpenFiles()==2);directory.reset();assert(tab5SdOpenFiles()==1);child.reset();assert(tab5SdOpenFiles()==0);}
 {auto usb=tab5FS.open("/USB/test");assert(usb);assert(tab5SdOpenFiles()==0);assert(Tab5UsbStorage::references==1);}
 assert(Tab5UsbStorage::references==0);
 assert(tab5FsRevision()==initialRevision); // Reading and browsing never request a refresh.
 {auto writable=tab5FS.open("/empty","w");assert(tab5FsRevision()==initialRevision);
  auto shared=writable;writable.reset();assert(tab5FsRevision()==initialRevision);
  shared->close();assert(tab5FsRevision()==initialRevision+1);shared->close();}
 assert(tab5FsRevision()==initialRevision+1); // Truncate/create publishes once on final close.
 {auto writable=tab5FS.open("/USB/test","a");const uint8_t byte=42;
  assert(writable->write(&byte,1)==1);assert(writable->write(&byte,1)==1);
  assert(tab5FsRevision()==initialRevision+1);}
 assert(tab5FsRevision()==initialRevision+2);
 auto revision=tab5FsRevision();
 assert(tab5FS.mkdir("/new"));assert(tab5FsRevision()==++revision);
 assert(tab5FS.rename("/new","/renamed"));assert(tab5FsRevision()==++revision);
 assert(tab5FS.rmdir("/renamed"));assert(tab5FsRevision()==++revision);
 assert(tab5FS.remove("/one"));assert(tab5FsRevision()==++revision);
 assert(!tab5FS.remove("/USB"));assert(tab5FsRevision()==revision);
 assert(!tab5FS.rename("/one","/USB/one"));assert(tab5FsRevision()==revision);
 tab5FsChanged();assert(tab5FsRevision()==++revision); // POSIX mutation integration.
 tab5SdSetAvailable(false);assert(!tab5FS.open("/one"));assert(!tab5FS.exists("/one"));assert(tab5SdOpenFiles()==0);
 {auto usb=tab5FS.open("/USB/test");assert(usb);} // SD ejection does not eject USB.
 tab5SdSetAvailable(true);
 {auto stale=tab5FS.open("/one");tab5SdSetAvailable(false);tab5SdSetAvailable(true);assert(!bool(*stale));}
 assert(tab5SdOpenFiles()==0);
 {Tab5StorageRecovery exclusive;assert(exclusive);assert(tab5StorageExclusiveOwner());
  auto probe=tab5FS.open("/probe");assert(probe);bool allowed=true;
  std::thread worker([&]{allowed=bool(tab5FS.open("/other"));});worker.join();assert(!allowed);}
 assert(tab5SdOpenFiles()==0);
 std::puts("PASS SD references/generations/exclusion; refresh revisions: reads stable, writes coalesced until close, SD/USB mutations, rejected mutations stable, POSIX notification");
}
