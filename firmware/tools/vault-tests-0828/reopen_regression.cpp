#include "Platform.h"
#include "Tab5KeyStore.h"
#include "Tab5Secrets.h"
#include "Tab5Environment.h"
#include <iostream>
int main(){
 unsigned checks=0,earlySuccess=0,extraIo=0;
 auto require=[&](bool value,const char* what){++checks;if(!value){std::cerr<<"FAIL: "<<what<<'\n';std::exit(2);}};
 require(Tab5KeyStore::initializeDevice("7391"),"initial PIN");
 require(Tab5Secrets::set("FIRST","fixture-one"),"first committed bank");
 require(Tab5Secrets::set("SECOND","fixture-two"),"second committed bank");
 const auto cleanA=files["/System/Secrets/credentials.a.t5v"]->data;
 const auto cleanB=files["/System/Secrets/credentials.b.t5v"]->data;
 for(unsigned cycle=0;cycle<32;++cycle){
  Tab5KeyStore::lock();
  files["/System/Secrets/credentials.a.t5v"]->data=cleanA;
  files["/System/Secrets/credentials.b.t5v"]->data=cleanB;
  files["/System/Secrets/credentials.a.t5v"]->data.back()^=1;
  files["/System/Secrets/credentials.b.t5v"]->data.back()^=1;
  require(Tab5KeyStore::beginDeviceUnlock("7391"),"schedule damaged-bank unlock");
  fsObserver=[&](){if(!strcmp(Tab5KeyStore::status(),"Coffre déverrouillé"))++earlySuccess;};
  pump();fsObserver={};
  require(Tab5KeyStore::unlocked(),"PIN still unlocks encryption keys");
  const auto beforeA=files["/System/Secrets/credentials.a.t5v"]->data;
  const auto beforeB=files["/System/Secrets/credentials.b.t5v"]->data;
  int previous=fsOps;
  for(unsigned poll=0;poll<8;++poll){
   tab5_env_import_secrets();require(Tab5Secrets::names().empty(),"broken secrets remain closed");
   char value[513];require(!Tab5Secrets::get("FIRST",value,sizeof(value))&&!value[0],"no plaintext after failed preload");
  }
  extraIo+=unsigned(fsOps-previous);
  require(files["/System/Secrets/credentials.a.t5v"]->data==beforeA&&files["/System/Secrets/credentials.b.t5v"]->data==beforeB,"failed read preserves both encrypted banks");
  // A new unlocked session can recover when the underlying card is repaired.
  Tab5KeyStore::lock();files["/System/Secrets/credentials.a.t5v"]->data=cleanA;files["/System/Secrets/credentials.b.t5v"]->data=cleanB;
  require(Tab5KeyStore::beginDeviceUnlock("7391"),"retry on next unlock");pump();
  require(Tab5Secrets::names().size()==2,"repaired banks recover on next unlock");
 }
 std::cout<<"{\"checks\":"<<checks<<",\"failedThenRepairedSessions\":32,\"earlySuccessDuringPreload\":"<<earlySuccess<<",\"uiFileOperationsAfterFailedPreload\":"<<extraIo<<"}\n";
 return earlySuccess||extraIo?1:0;
}
